#include "ftp_cmds.h"
#include "../net/socket_utils.h"
#include "../utils/utils.h"
#include "../transfer/data_transfer.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <sys/stat.h>
#include <limits.h>
#include <errno.h>

// 选择并打开一个 20000-65535 的监听端口（被动模式）
static int open_pasv_listener(uint16_t *out_port, int *out_fd)
{
    if (!out_port || !out_fd)
        return -1;

    static int seeded = 0;
    if (!seeded)
    {
        srand((unsigned)time(NULL) ^ (unsigned)getpid());
        seeded = 1;
    }

    const int min_p = 20000;
    const int max_p = 65535;
    const int range = max_p - min_p + 1;

    for (int attempt = 0; attempt < 64; attempt++)
    {
        uint16_t port = (uint16_t)(min_p + (rand() % range));

        int s = socket(AF_INET, SOCK_STREAM, 0);
        if (s < 0)
            continue;

        int reuse = 1;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        struct sockaddr_in sa;
        memset(&sa, 0, sizeof(sa));
        sa.sin_family = AF_INET;
        sa.sin_addr.s_addr = htonl(INADDR_ANY);
        sa.sin_port = htons(port);

        if (bind(s, (struct sockaddr *)&sa, sizeof(sa)) == 0)
        {
            if (listen(s, 1) == 0)
            {
                *out_port = port;
                *out_fd = s;
                return 0;
            }
        }
        close(s);
    }
    return -1;
}

static void *xfer_thread(void *arg)
{
    XferTask *task = (XferTask *)arg;
    ClientConn *conn = task->conn;
    // 标记响应 150
    socket_send(conn->ctrl_fd, "150 Opening data connection.");
    // 建立数据连接（PORT 主动 connect / PASV accept）
    if (transfer_init_data_conn(conn) != 0)
    {
        socket_send(conn->ctrl_fd, "425 Can't open data connection.");
        conn->xfer_in_progress = 0;
        free(task);
        return NULL;
    }
    int rc = -1;
    if (task->type == XFER_RETR)
    {
        rc = transfer_send_file(conn, task->filename);
    }
    else if (task->type == XFER_LIST)
    {
        rc = transfer_send_list(conn);
    }
    else if (task->type == XFER_STOR)
    {
        rc = transfer_recv_file(conn, task->filename);
    }
    // 关闭数据连接与复位状态
    transfer_close_data_conn(conn);
    if (rc == 0)
    {
        socket_send(conn->ctrl_fd, "226 Transfer complete.");
    }
    else
    {
        if (task->type == XFER_RETR)
            socket_send(conn->ctrl_fd, "451 Requested action aborted: local error in processing.");
        else if (task->type == XFER_STOR)
            socket_send(conn->ctrl_fd, "550 Failed to create or write file.");
        else if (task->type == XFER_LIST)
            socket_send(conn->ctrl_fd, "451 Requested action aborted: local error in processing.");
    }
    conn->xfer_in_progress = 0;
    free(task);
    return NULL;
}

// 解析 CWD 参数为磁盘绝对路径，限制在 root_dir 内
static int resolve_abs_path(ClientConn *conn, const char *arg, char *abs_path, size_t len)
{
    if (!conn || !arg || !abs_path || len == 0)
        return -1;

    // printf("[RESOLVE] root='%s' cwd='%s' raw='%s'\n", conn->root_dir, conn->current_dir, arg);

    // trim
    while (*arg == ' ' || *arg == '\t')
        arg++;
    size_t alen = strlen(arg);
    while (alen > 0 && (arg[alen - 1] == ' ' || arg[alen - 1] == '\t' || arg[alen - 1] == '\r' || arg[alen - 1] == '\n'))
        alen--;
    if (alen == 0)
        return -1;

    char trimmed[PATH_MAX];
    if (alen >= sizeof(trimmed))
        return -1;
    memcpy(trimmed, arg, alen);
    trimmed[alen] = '\0';
    // printf("[RESOLVE] trimmed='%s' (len=%zu)\n", trimmed, alen);

    char tmp[PATH_MAX];
    if (trimmed[0] == '/')
    {
        // 绝对路径：相对于 FTP 根目录（跳过前导'/'）
        const char *rel = trimmed + 1;
        if (rel[0] == '\0')
        {
            snprintf(tmp, sizeof(tmp), "%s", conn->root_dir);
            // printf("[RESOLVE] absolute to root -> '%s'\n", tmp);
        }
        else
        {
            // printf("[RESOLVE] absolute join: base='%s' rel='%s'\n", conn->root_dir, rel);
            if (!utils_join_path(conn->root_dir, rel, tmp, sizeof(tmp)))
            {
                // printf("[RESOLVE] utils_join_path failed (abs): base='%s' rel='%s'\n", conn->root_dir, rel);
                return -1;
            }
            // printf("[RESOLVE] joined(abs)='%s'\n", tmp);
        }
    }
    else
    {
        // 相对路径：基于当前目录
        // printf("[RESOLVE] relative join: base='%s' rel='%s'\n", conn->current_dir, trimmed);
        if (!utils_join_path(conn->current_dir, trimmed, tmp, sizeof(tmp)))
        {
            // printf("[RESOLVE] utils_join_path failed (rel): base='%s' rel='%s'\n", conn->current_dir, trimmed);
            return -1;
        }
        // printf("[RESOLVE] joined(rel)='%s'\n", tmp);
    }

    // 安全检查：目标必须在 root_dir 内
    if (utils_check_path(conn->root_dir, tmp) == 0)
    {
        // printf("[RESOLVE] utils_check_path denied: root='%s' target='%s' \n", conn->root_dir, tmp);
        return -1;
    }
    snprintf(abs_path, len, "%s", tmp);
    // printf("[RESOLVE] final='%s'\n", abs_path);
    return 0;
}

// 将绝对磁盘路径转换为 FTP 显示路径（相对 root_dir，根显示为 "/"）
static void to_ftp_display_path(ClientConn *conn, const char *abs_path, char *out, size_t len)
{
    size_t rlen = strlen(conn->root_dir);
    if (strncmp(abs_path, conn->root_dir, rlen) == 0)
    {
        const char *rel = abs_path + rlen;
        if (*rel == '\0')
        {
            // 正好是根目录
            snprintf(out, len, "/");
        }
        else
        {
            // 确保前面带一个 '/'
            if (*rel != '/')
                snprintf(out, len, "/%s", rel);
            else
                snprintf(out, len, "%s", rel);
        }
    }
    else
    {
        // 兜底（不应出现）
        snprintf(out, len, "/");
    }
}

static void cmd_handle_user(ClientConn *conn, const char *args)
{
    // 检查是否已认证
    if (conn->auth_state == AUTH_STATE_AUTHED)
    {
        socket_send(conn->ctrl_fd, "503 Already logged in.");
        return;
    }

    // 验证用户名是否为anonymous
    if (args == NULL || strcmp(args, "anonymous") != 0)
    {
        socket_send(conn->ctrl_fd, "530 Only anonymous login supported.");
        return;
    }

    // 接受匿名用户，提示输入密码
    conn->pending_user_anon = 1;
    socket_send(conn->ctrl_fd, "331 Please specify the password.");
    return;
}

static void cmd_handle_pass(ClientConn *conn, const char *args)
{
    // 检查认证状态
    if (conn->auth_state == AUTH_STATE_AUTHED)
    {
        socket_send(conn->ctrl_fd, "230 Already logged in.");
        return;
    }

    // 简单验证密码不为空(实际匿名登录通常不严格验证邮箱格式)
    if (!conn->pending_user_anon)
    {
        socket_send(conn->ctrl_fd, "503 Login with USER anonymous first.");
        return;
    }

    if (args == NULL || strlen(args) == 0)
    {
        socket_send(conn->ctrl_fd, "501 Password required.");
        return;
    }

    // 标记为已认证并发送成功消息
    conn->auth_state = AUTH_STATE_AUTHED;
    conn->pending_user_anon = 0;
    socket_send(conn->ctrl_fd, "230 Login successful.");
    return;
}

static void cmd_handle_port(ClientConn *conn, const char *args)
{
    if (conn->auth_state != AUTH_STATE_AUTHED)
    {
        socket_send(conn->ctrl_fd, "530 Please login with USER and PASS.");
        return;
    }

    char ip[INET_ADDRSTRLEN];
    uint16_t port = 0;
    if (parse_port_arg(args, ip, sizeof(ip), &port) != 0)
    {
        socket_send(conn->ctrl_fd, "501 Syntax error in parameters or arguments.");
        return;
    }

    // printf("Client%d PORT Mode: IP: %s, Port: %u\n", conn->ctrl_fd, ip, port);

    // RFC语义：收到新的PORT时，停止任何被动监听并丢弃已有数据连接
    if (conn->data_fd >= 3)
    {
        socket_close(conn->data_fd);
    }
    conn->data_fd = -1;

    if (conn->pasv_listen_fd >= 3)
    {
        socket_close(conn->pasv_listen_fd);
    }
    conn->pasv_listen_fd = -1;

    // 切换到主动模式，保存目标
    conn->data_mode = DATA_MODE_PORT;
    snprintf(conn->data_host, sizeof(conn->data_host), "%s", ip);
    conn->data_port = port;

    // 确认
    socket_send(conn->ctrl_fd, "200 PORT command successful.");
}

static void cmd_handle_pasv(ClientConn *conn, const char *args)
{
    (void)args;
    if (conn->auth_state != AUTH_STATE_AUTHED)
    {
        socket_send(conn->ctrl_fd, "530 Please login with USER and PASS.");
        return;
    }

    // 关闭已有数据连接与旧的PASV监听（RFC建议）
    if (conn->data_fd >= 3)
    {
        socket_close(conn->data_fd);
    }
    conn->data_fd = -1;
    if (conn->pasv_listen_fd >= 3)
    {
        socket_close(conn->pasv_listen_fd);
    }
    conn->pasv_listen_fd = -1;

    // 打开新的被动监听端口
    uint16_t pasv_port = 0;
    int pasv_fd = -1;
    if (open_pasv_listener(&pasv_port, &pasv_fd) != 0)
    {
        socket_send(conn->ctrl_fd, "425 Can't open passive connection.");
        return;
    }

    // 获取服务器在控制连接上的本地IP，作为PASV应答中的 h1..h4
    struct sockaddr_in local;
    socklen_t llen = sizeof(local);
    memset(&local, 0, sizeof(local));
    if (getsockname(conn->ctrl_fd, (struct sockaddr *)&local, &llen) != 0)
    {
        socket_close(pasv_fd);
        socket_send(conn->ctrl_fd, "425 Can't determine server address.");
        return;
    }

    uint32_t addr = ntohl(local.sin_addr.s_addr);
    unsigned h1 = (addr >> 24) & 0xFF;
    unsigned h2 = (addr >> 16) & 0xFF;
    unsigned h3 = (addr >> 8) & 0xFF;
    unsigned h4 = (addr) & 0xFF;
    unsigned p1 = (pasv_port >> 8) & 0xFF;
    unsigned p2 = (pasv_port) & 0xFF;

    // 保存会话状态
    conn->data_mode = DATA_MODE_PASV;
    conn->pasv_listen_fd = pasv_fd;

    // 返回 227（按建议格式：前面带 '='）
    char resp[128];
    snprintf(resp, sizeof(resp), "227 =%u,%u,%u,%u,%u,%u", h1, h2, h3, h4, p1, p2);
    socket_send(conn->ctrl_fd, resp);
}

static void cmd_handle_syst(ClientConn *conn, const char *args)
{
    (void)args;
    socket_send(conn->ctrl_fd, "215 UNIX Type: L8");
}

static void cmd_handle_type(ClientConn *conn, const char *args)
{
    // 只接受 TYPE I，其它参数返回错误
    if (!args)
    {
        socket_send(conn->ctrl_fd, "501 Syntax error in parameters or arguments.");
        return;
    }

    // 跳过前后空白
    while (*args == ' ' || *args == '\t')
        args++;
    const char *end = args + strlen(args);
    while (end > args && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n'))
        end--;

    // 空参数 -> 501
    if (end <= args)
    {
        socket_send(conn->ctrl_fd, "501 Syntax error in parameters or arguments.");
        return;
    }

    // 仅取第一个标记进行比较
    const char *p = args;
    while (p < end && *p != ' ' && *p != '\t')
        p++;
    size_t len = (size_t)(p - args);

    // 构造参数片段进行比较
    char param[8];
    if (len >= sizeof(param))
        len = sizeof(param) - 1;
    memcpy(param, args, len);
    param[len] = '\0';

    if (strcasecmp(param, "I") == 0)
    {
        socket_send(conn->ctrl_fd, "200 Type set to I.");
        return;
    }

    // 不支持的 TYPE 参数
    socket_send(conn->ctrl_fd, "504 Command not implemented for that parameter.");
}

static void cmd_handle_retr(ClientConn *conn, const char *args)
{
    if (conn->auth_state != AUTH_STATE_AUTHED)
    {
        socket_send(conn->ctrl_fd, "530 Please login with USER and PASS.");
        return;
    }
    // 必须先 PORT 或 PASV
    if (conn->data_mode == DATA_MODE_NONE)
    {
        socket_send(conn->ctrl_fd, "425 Use PORT or PASV first.");
        return;
    }
    // 参数检查
    if (!args || args[0] == '\0')
    {
        socket_send(conn->ctrl_fd, "501 Syntax error in parameters or arguments.");
        return;
    }
    if (conn->xfer_in_progress)
    {
        // 已有传输在进行，直接忽略或提示忙
        socket_send(conn->ctrl_fd, "450 Another transfer is in progress.");
        return;
    }
    // 启动传输线程
    XferTask *task = (XferTask *)malloc(sizeof(XferTask));
    if (!task)
    {
        socket_send(conn->ctrl_fd, "451 Local error: out of memory.");
        return;
    }
    task->conn = conn;
    task->type = XFER_RETR;
    // 保存文件名（由 data_transfer 内部做路径拼接与限制）
    snprintf(task->filename, sizeof(task->filename), "%s", args);
    conn->xfer_in_progress = 1;
    pthread_t th;
    if (pthread_create(&th, NULL, xfer_thread, task) != 0)
    {
        conn->xfer_in_progress = 0;
        free(task);
        socket_send(conn->ctrl_fd, "451 Local error: cannot start transfer.");
        return;
    }
    pthread_detach(th);
}

static void cmd_handle_stor(ClientConn *conn, const char *args)
{
    if (conn->auth_state != AUTH_STATE_AUTHED)
    {
        socket_send(conn->ctrl_fd, "530 Please login with USER and PASS.");
        return;
    }
    if (conn->data_mode == DATA_MODE_NONE)
    {
        socket_send(conn->ctrl_fd, "425 Use PORT or PASV first.");
        return;
    }
    if (!args || args[0] == '\0')
    {
        socket_send(conn->ctrl_fd, "501 Syntax error in parameters or arguments.");
        return;
    }
    if (conn->xfer_in_progress)
    {
        socket_send(conn->ctrl_fd, "450 Another transfer is in progress.");
        return;
    }
    XferTask *task = (XferTask *)malloc(sizeof(XferTask));
    if (!task)
    {
        socket_send(conn->ctrl_fd, "451 Local error: out of memory.");
        return;
    }
    task->conn = conn;
    task->type = XFER_STOR;
    snprintf(task->filename, sizeof(task->filename), "%s", args);
    conn->xfer_in_progress = 1;
    pthread_t th;
    if (pthread_create(&th, NULL, xfer_thread, task) != 0)
    {
        conn->xfer_in_progress = 0;
        free(task);
        socket_send(conn->ctrl_fd, "451 Local error: cannot start transfer.");
        return;
    }
    pthread_detach(th);
}

static void cmd_handle_cwd(ClientConn *conn, const char *args)
{
    if (conn->auth_state != AUTH_STATE_AUTHED)
    {
        socket_send(conn->ctrl_fd, "530 Please login with USER and PASS.");
        return;
    }
    if (!args || args[0] == '\0')
    {
        socket_send(conn->ctrl_fd, "501 Syntax error in parameters or arguments.");
        return;
    }

    // 解析目标路径合法性（不允许超出根目录）
    char target[PATH_MAX];
    if (resolve_abs_path(conn, args, target, sizeof(target)) != 0)
    {
        socket_send(conn->ctrl_fd, "550 Failed to change directory.");
        return;
    }

    // 检查目标路径是否为真实存在的目录
    struct stat st;
    if (stat(target, &st) != 0 || !S_ISDIR(st.st_mode))
    {
        socket_send(conn->ctrl_fd, "550 Failed to change directory.");
        return;
    }
    snprintf(conn->current_dir, sizeof(conn->current_dir), "%s", target);
    socket_send(conn->ctrl_fd, "250 Directory successfully changed.");
}

static void cmd_handle_pwd(ClientConn *conn, const char *args)
{
    (void)args;
    if (conn->auth_state != AUTH_STATE_AUTHED)
    {
        socket_send(conn->ctrl_fd, "530 Please login with USER and PASS.");
        return;
    }

    char disp[PATH_MAX];
    to_ftp_display_path(conn, conn->current_dir, disp, sizeof(disp));

    char line[PATH_MAX + 32];
    snprintf(line, sizeof(line), "257 \"%s\"", disp);
    socket_send(conn->ctrl_fd, line);
}

static void cmd_handle_mkd(ClientConn *conn, const char *args)
{
    if (conn->auth_state != AUTH_STATE_AUTHED)
    {
        socket_send(conn->ctrl_fd, "530 Please login with USER and PASS.");
        return;
    }
    if (!args || args[0] == '\0')
    {
        socket_send(conn->ctrl_fd, "501 Syntax error in parameters or arguments.");
        return;
    }

    // trim 参数前后空白
    while (*args == ' ' || *args == '\t')
        args++;
    const char *end = args + strlen(args);
    while (end > args && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n'))
        end--;
    if (end <= args)
    {
        socket_send(conn->ctrl_fd, "501 Syntax error in parameters or arguments.");
        return;
    }
    char trimmed[PATH_MAX];
    size_t tlen = (size_t)(end - args);
    if (tlen >= sizeof(trimmed))
        tlen = sizeof(trimmed) - 1;
    memcpy(trimmed, args, tlen);
    trimmed[tlen] = '\0';

    // 计算目标路径（绝对: 相对root；相对: 相对current）
    char target[PATH_MAX];
    if (trimmed[0] == '/')
    {
        const char *rel = trimmed + 1; // 去掉前导'/'
        if (!utils_join_path(conn->root_dir, rel, target, sizeof(target)))
        {
            socket_send(conn->ctrl_fd, "550 Create directory operation failed.");
            return;
        }
    }
    else
    {
        if (!utils_join_path(conn->current_dir, trimmed, target, sizeof(target)))
        {
            socket_send(conn->ctrl_fd, "550 Create directory operation failed.");
            return;
        }
    }

    // 计算父目录，确保父目录存在且在 root 内
    char parent[PATH_MAX];
    snprintf(parent, sizeof(parent), "%s", target);

    // 去掉末尾的 '/'（若有）
    size_t plen = strlen(parent);
    while (plen > 1 && parent[plen - 1] == '/')
    {
        parent[--plen] = '\0';
    }

    // 定位父目录分隔符
    char *slash = strrchr(parent, '/');
    if (slash == NULL)
    {
        // 理论不该发生（绝对路径至少有一个前导'/'）
        socket_send(conn->ctrl_fd, "550 Create directory operation failed.");
        return;
    }
    if (slash == parent)
    {
        // 父目录就是根目录
        parent[1] = '\0';
        // 映射到实际磁盘根路径
        snprintf(parent, sizeof(parent), "%s", conn->root_dir);
    }
    else
    {
        // 截断到父目录
        *slash = '\0';
    } // 父目录安全与存在性校验
    // 注意：此项目中 utils_check_path 在其他地方是用 "== 0 表示拒绝" 的约定，保持一致
    if (utils_check_path(conn->root_dir, parent) == 0)
    {
        socket_send(conn->ctrl_fd, "550 Create directory operation failed.");
        return;
    }
    struct stat pst;
    if (stat(parent, &pst) != 0 || !S_ISDIR(pst.st_mode))
    {
        socket_send(conn->ctrl_fd, "550 Create directory operation failed.");
        return;
    }

    // 创建目录
    if (mkdir(target, 0755) != 0)
    {
        socket_send(conn->ctrl_fd, "550 Create directory operation failed.");
        return;
    }

    // 成功：返回 257 "<显示路径>"
    char disp[PATH_MAX];
    to_ftp_display_path(conn, target, disp, sizeof(disp));

    char line[PATH_MAX + 64];
    snprintf(line, sizeof(line), "257 \"%s\"", disp);
    socket_send(conn->ctrl_fd, line);
}

static void cmd_handle_rmd(ClientConn *conn, const char *args)
{
    if (conn->auth_state != AUTH_STATE_AUTHED)
    {
        socket_send(conn->ctrl_fd, "530 Please login with USER and PASS.");
        return;
    }
    if (!args || args[0] == '\0')
    {
        socket_send(conn->ctrl_fd, "501 Syntax error in parameters or arguments.");
        return;
    }

    // trim 参数
    while (*args == ' ' || *args == '\t')
        args++;
    const char *end = args + strlen(args);
    while (end > args && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n'))
        end--;
    if (end <= args)
    {
        socket_send(conn->ctrl_fd, "501 Syntax error in parameters or arguments.");
        return;
    }
    char trimmed[PATH_MAX];
    size_t tlen = (size_t)(end - args);
    if (tlen >= sizeof(trimmed))
        tlen = sizeof(trimmed) - 1;
    memcpy(trimmed, args, tlen);
    trimmed[tlen] = '\0';
    // printf("[RMD] trimmed='%s'\n", trimmed);

    // 解析目标路径（绝对: 相对 root；相对: 相对 current）
    char target[PATH_MAX];
    if (trimmed[0] == '/')
    {
        const char *rel = trimmed + 1; // 去掉前导 '/'
        // printf("[RMD] join(abs): base='%s' rel='%s'\n", conn->root_dir, rel);
        if (!utils_join_path(conn->root_dir, rel, target, sizeof(target)))
        {
            // printf("[RMD] join(abs) failed\n");
            socket_send(conn->ctrl_fd, "550 Remove directory operation failed.");
            return;
        }
    }
    else
    {
        // printf("[RMD] join(rel): base='%s' rel='%s'\n", conn->current_dir, trimmed);
        if (!utils_join_path(conn->current_dir, trimmed, target, sizeof(target)))
        {
            // printf("[RMD] join(rel) failed\n");
            socket_send(conn->ctrl_fd, "550 Remove directory operation failed.");
            return;
        }
    }
    // printf("[RMD] target='%s'\n", target);

    // printf("[RMD] check_path(root='%s', target='%s') => %d (0=deny)\n",
    //        conn->root_dir, target, utils_check_path(conn->root_dir, target));
    // 安全校验：必须在 root 内，且不能是根目录本身
    if (utils_check_path(conn->root_dir, target) == 0)
    {
        socket_send(conn->ctrl_fd, "550 Remove directory operation failed.");
        return;
    }
    if (strcmp(target, conn->root_dir) == 0)
    {
        socket_send(conn->ctrl_fd, "550 Cannot remove root directory.");
        return;
    }

    // 必须存在且为目录
    struct stat st;
    if (stat(target, &st) != 0 || !S_ISDIR(st.st_mode))
    {
        // int se = errno;
        // printf("[RMD] stat failed or not dir: path='%s' errno=%d (%s) is_dir=%d\n",
        //        target, se, strerror(se), S_ISDIR(st.st_mode));
        socket_send(conn->ctrl_fd, "550 Remove directory operation failed.");
        return;
    }

    // 删除（要求为空目录）
    if (rmdir(target) != 0)
    {
        // int re = errno;
        // printf("[RMD] rmdir failed: path='%s' errno=%d (%s)\n",
        //        target, re, strerror(re));
        socket_send(conn->ctrl_fd, "550 Remove directory operation failed.");
        return;
    }

    socket_send(conn->ctrl_fd, "250 Directory removed.");
}

static void cmd_handle_list(ClientConn *conn, const char *args)
{
    (void)args; // 本需求按“列出当前目录”处理，忽略参数
    if (conn->auth_state != AUTH_STATE_AUTHED)
    {
        socket_send(conn->ctrl_fd, "530 Please login with USER and PASS.");
        return;
    }
    if (conn->data_mode == DATA_MODE_NONE)
    {
        socket_send(conn->ctrl_fd, "425 Use PORT or PASV first.");
        return;
    }
    if (conn->xfer_in_progress)
    {
        socket_send(conn->ctrl_fd, "450 Another transfer is in progress.");
        return;
    }
    XferTask *task = (XferTask *)malloc(sizeof(XferTask));
    if (!task)
    {
        socket_send(conn->ctrl_fd, "451 Local error: out of memory.");
        return;
    }
    task->conn = conn;
    task->type = XFER_LIST;
    task->filename[0] = '\0'; // LIST 无需文件名
    conn->xfer_in_progress = 1;
    pthread_t th;
    if (pthread_create(&th, NULL, xfer_thread, task) != 0)

    {
        conn->xfer_in_progress = 0;
        free(task);
        socket_send(conn->ctrl_fd, "451 Local error: cannot start transfer.");
        return;
    }
    pthread_detach(th);
}

/**
 * 处理客户端发送的命令行
 * @param conn 客户端连接信息结构体指针
 * @param cmd 客户端发送的命令字符串
 * @param args 客户端发送的命令参数字符串
 */
void cmd_process(ClientConn *conn, const char *cmd, const char *args)
{
    if (!conn || !cmd)
    {
        if (conn)
            socket_send(conn->ctrl_fd, "500 Internal error.");
        return;
    }

    // 阶段1：未登录，且未进入匿名流程 -> 仅允许 USER
    if (conn->auth_state != AUTH_STATE_AUTHED && conn->pending_user_anon == 0)
    {
        if (strcmp(cmd, "USER") == 0)
        {
            cmd_handle_user(conn, args);
            return;
        }
        // 其他命令一律不合法
        socket_send(conn->ctrl_fd, "530 Please login with USER anonymous.");
        return;
    }

    // 阶段2：已收到 USER anonymous，等待 PASS -> 仅允许 PASS
    if (conn->auth_state != AUTH_STATE_AUTHED && conn->pending_user_anon == 1)
    {
        if (strcmp(cmd, "PASS") == 0)
        {
            cmd_handle_pass(conn, args);
            return;
        }
        // USER 在该阶段关闭，始终提示 PASS 验证
        socket_send(conn->ctrl_fd, "331 User accepted, send PASS (email address).");
        return;
    }

    // 阶段3：已登录，开放其他命令
    if (strcmp(cmd, "PORT") == 0)
    {
        cmd_handle_port(conn, args);
        return;
    }
    else if (strcmp(cmd, "PASV") == 0)
    {
        cmd_handle_pasv(conn, args);
        return;
    }
    else if (strcmp(cmd, "SYST") == 0)
    {
        cmd_handle_syst(conn, args);
        return;
    }
    else if (strcmp(cmd, "TYPE") == 0)
    {
        cmd_handle_type(conn, args);
        return;
    }
    else if (strcmp(cmd, "RETR") == 0)
    {
        cmd_handle_retr(conn, args);
        return;
    }
    else if (strcmp(cmd, "STOR") == 0)
    {
        cmd_handle_stor(conn, args);
        return;
    }
    else if (strcmp(cmd, "CWD") == 0)
    {
        cmd_handle_cwd(conn, args);
        return;
    }
    else if (strcmp(cmd, "PWD") == 0)
    {
        cmd_handle_pwd(conn, args);
        return;
    }
    else if (strcmp(cmd, "MKD") == 0)
    {
        cmd_handle_mkd(conn, args);
        return;
    }
    else if (strcmp(cmd, "RMD") == 0)
    {
        cmd_handle_rmd(conn, args);
        return;
    }
    else if (strcmp(cmd, "LIST") == 0)
    {
        cmd_handle_list(conn, args);
        return;
    }
    socket_send(conn->ctrl_fd, "502 Command not implemented.");
}
