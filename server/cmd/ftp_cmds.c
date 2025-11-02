#include "ftp_cmds.h"
#include "../net/socket_utils.h"
#include "../../utils/utils.h"
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
    socket_send(conn->ctrl_fd, "150 Opening data connection.\r\n");
    // 建立数据连接（PORT 主动 connect / PASV accept）
    if (transfer_init_data_conn(conn) != 0)
    {
        socket_send(conn->ctrl_fd, "425 Can't open data connection.\r\n");
        conn->xfer_in_progress = 0;
        free(task);
        return NULL;
    }
    int rc = -1;
    if (task->type == XFER_RETR)
    {
        rc = transfer_send_file(conn, task->filename);
    }
    else
    {
        rc = transfer_recv_file(conn, task->filename);
    }
    // 关闭数据连接与复位状态
    transfer_close_data_conn(conn);
    if (rc == 0)
    {
        socket_send(conn->ctrl_fd, "226 Transfer complete.\r\n");
    }
    else
    {
        if (task->type == XFER_RETR)
            socket_send(conn->ctrl_fd, "451 Requested action aborted: local error in processing.\r\n");
        else
            socket_send(conn->ctrl_fd, "550 Failed to create or write file.\r\n");
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

    printf("[RESOLVE] root='%s' cwd='%s' raw='%s'\n", conn->root_dir, conn->current_dir, arg);

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
    printf("[RESOLVE] trimmed='%s' (len=%zu)\n", trimmed, alen);

    char tmp[PATH_MAX];
    if (trimmed[0] == '/')
    {
        // 绝对路径：相对于 FTP 根目录（跳过前导'/'）
        const char *rel = trimmed + 1;
        if (rel[0] == '\0')
        {
            strncpy(tmp, conn->root_dir, sizeof(tmp) - 1);
            tmp[sizeof(tmp) - 1] = '\0';
            printf("[RESOLVE] absolute to root -> '%s'\n", tmp);
        }
        else
        {
            printf("[RESOLVE] absolute join: base='%s' rel='%s'\n", conn->root_dir, rel);
            if (!utils_join_path(conn->root_dir, rel, tmp, sizeof(tmp)))
            {
                printf("[RESOLVE] utils_join_path failed (abs): base='%s' rel='%s'\n", conn->root_dir, rel);
                return -1;
            }
            printf("[RESOLVE] joined(abs)='%s'\n", tmp);
        }
    }
    else
    {
        // 相对路径：基于当前目录
        printf("[RESOLVE] relative join: base='%s' rel='%s'\n", conn->current_dir, trimmed);
        if (!utils_join_path(conn->current_dir, trimmed, tmp, sizeof(tmp)))
        {
            printf("[RESOLVE] utils_join_path failed (rel): base='%s' rel='%s'\n", conn->current_dir, trimmed);
            return -1;
        }
        printf("[RESOLVE] joined(rel)='%s'\n", tmp);
    }

    // 安全检查：目标必须在 root_dir 内
    if (utils_check_path(conn->root_dir, tmp) == 0)
    {
        printf("[RESOLVE] utils_check_path denied: root='%s' target='%s' \n", conn->root_dir, tmp);
        return -1;
    }

    strncpy(abs_path, tmp, len - 1);
    abs_path[len - 1] = '\0';
    printf("[RESOLVE] final='%s'\n", abs_path);
    return 0;
}

static void cmd_handle_user(ClientConn *conn, const char *args)
{
    // 检查是否已认证
    if (conn->auth_state == AUTH_STATE_AUTHED)
    {
        socket_send(conn->ctrl_fd, "503 Already logged in.\r\n");
        return;
    }

    // 验证用户名是否为anonymous
    if (args == NULL || strcmp(args, "anonymous") != 0)
    {
        socket_send(conn->ctrl_fd, "530 Only anonymous login supported.\r\n");
        return;
    }

    // 接受匿名用户，提示输入密码(邮箱)
    conn->pending_user_anon = 1;
    socket_send(conn->ctrl_fd, "331 Please specify the password (email address).\r\n");
    return;
}

static void cmd_handle_pass(ClientConn *conn, const char *args)
{
    // 检查认证状态
    if (conn->auth_state == AUTH_STATE_AUTHED)
    {
        socket_send(conn->ctrl_fd, "230 Already logged in.\r\n");
        return;
    }

    // 简单验证密码不为空(实际匿名登录通常不严格验证邮箱格式)
    if (!conn->pending_user_anon)
    {
        socket_send(conn->ctrl_fd, "503 Login with USER anonymous first.\r\n");
        return;
    }

    if (args == NULL || strlen(args) == 0)
    {
        socket_send(conn->ctrl_fd, "501 Password required (email address).\r\n");
        return;
    }

    // 标记为已认证并发送成功消息
    conn->auth_state = AUTH_STATE_AUTHED;
    conn->pending_user_anon = 0;
    socket_send(conn->ctrl_fd, "230 Login successful.\r\n");
    return;
}

static void cmd_handle_port(ClientConn *conn, const char *args)
{
    if (conn->auth_state != AUTH_STATE_AUTHED)
    {
        socket_send(conn->ctrl_fd, "530 Please login with USER and PASS.\r\n");
        return;
    }

    char ip[INET_ADDRSTRLEN];
    uint16_t port = 0;
    if (parse_port_arg(args, ip, sizeof(ip), &port) != 0)
    {
        socket_send(conn->ctrl_fd, "501 Syntax error in parameters or arguments.\r\n");
        return;
    }

    printf("Client%d PORT Mode: IP: %s, Port: %u\n", conn->ctrl_fd, ip, port);

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
    strncpy(conn->data_host, ip, sizeof(conn->data_host) - 1);
    conn->data_host[sizeof(conn->data_host) - 1] = '\0';
    conn->data_port = port;

    // 确认
    socket_send(conn->ctrl_fd, "200 PORT command successful.\r\n");
}

static void cmd_handle_pasv(ClientConn *conn, const char *args)
{
    (void)args;
    if (conn->auth_state != AUTH_STATE_AUTHED)
    {
        socket_send(conn->ctrl_fd, "530 Please login with USER and PASS.\r\n");
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
        socket_send(conn->ctrl_fd, "425 Can't open passive connection.\r\n");
        return;
    }

    // 获取服务器在控制连接上的本地IP，作为PASV应答中的 h1..h4
    struct sockaddr_in local;
    socklen_t llen = sizeof(local);
    memset(&local, 0, sizeof(local));
    if (getsockname(conn->ctrl_fd, (struct sockaddr *)&local, &llen) != 0)
    {
        socket_close(pasv_fd);
        socket_send(conn->ctrl_fd, "425 Can't determine server address.\r\n");
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
    snprintf(resp, sizeof(resp), "227 =%u,%u,%u,%u,%u,%u\r\n", h1, h2, h3, h4, p1, p2);
    socket_send(conn->ctrl_fd, resp);
}

static void cmd_handle_syst(ClientConn *conn, const char *args)
{
    (void)args;
    socket_send(conn->ctrl_fd, "215 UNIX Type: L8\r\n");
}

static void cmd_handle_type(ClientConn *conn, const char *args)
{
    // 只接受 TYPE I，其它参数返回错误
    if (!args)
    {
        socket_send(conn->ctrl_fd, "501 Syntax error in parameters or arguments.\r\n");
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
        socket_send(conn->ctrl_fd, "501 Syntax error in parameters or arguments.\r\n");
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
        socket_send(conn->ctrl_fd, "200 Type set to I.\r\n");
        return;
    }

    // 不支持的 TYPE 参数
    socket_send(conn->ctrl_fd, "504 Command not implemented for that parameter.\r\n");
}

static void cmd_handle_retr(ClientConn *conn, const char *args)
{
    if (conn->auth_state != AUTH_STATE_AUTHED)
    {
        socket_send(conn->ctrl_fd, "530 Please login with USER and PASS.\r\n");
        return;
    }
    // 必须先 PORT 或 PASV
    if (conn->data_mode == DATA_MODE_NONE)
    {
        socket_send(conn->ctrl_fd, "425 Use PORT or PASV first.\r\n");
        return;
    }
    // 参数检查
    if (!args || args[0] == '\0')
    {
        socket_send(conn->ctrl_fd, "501 Syntax error in parameters or arguments.\r\n");
        return;
    }
    if (conn->xfer_in_progress)
    {
        // 已有传输在进行，直接忽略或提示忙
        socket_send(conn->ctrl_fd, "450 Another transfer is in progress.\r\n");
        return;
    }
    // 启动传输线程
    XferTask *task = (XferTask *)malloc(sizeof(XferTask));
    if (!task)
    {
        socket_send(conn->ctrl_fd, "451 Local error: out of memory.\r\n");
        return;
    }
    task->conn = conn;
    task->type = XFER_RETR;
    // 保存文件名（由 data_transfer 内部做路径拼接与限制）
    strncpy(task->filename, args, sizeof(task->filename) - 1);
    task->filename[sizeof(task->filename) - 1] = '\0';
    conn->xfer_in_progress = 1;
    pthread_t th;
    if (pthread_create(&th, NULL, xfer_thread, task) != 0)
    {
        conn->xfer_in_progress = 0;
        free(task);
        socket_send(conn->ctrl_fd, "451 Local error: cannot start transfer.\r\n");
        return;
    }
    pthread_detach(th);
}

static void cmd_handle_stor(ClientConn *conn, const char *args)
{
    if (conn->auth_state != AUTH_STATE_AUTHED)
    {
        socket_send(conn->ctrl_fd, "530 Please login with USER and PASS.\r\n");
        return;
    }
    if (conn->data_mode == DATA_MODE_NONE)
    {
        socket_send(conn->ctrl_fd, "425 Use PORT or PASV first.\r\n");
        return;
    }
    if (!args || args[0] == '\0')
    {
        socket_send(conn->ctrl_fd, "501 Syntax error in parameters or arguments.\r\n");
        return;
    }
    if (conn->xfer_in_progress)
    {
        socket_send(conn->ctrl_fd, "450 Another transfer is in progress.\r\n");
        return;
    }
    XferTask *task = (XferTask *)malloc(sizeof(XferTask));
    if (!task)
    {
        socket_send(conn->ctrl_fd, "451 Local error: out of memory.\r\n");
        return;
    }
    task->conn = conn;
    task->type = XFER_STOR;
    strncpy(task->filename, args, sizeof(task->filename) - 1);
    task->filename[sizeof(task->filename) - 1] = '\0';
    conn->xfer_in_progress = 1;
    pthread_t th;
    if (pthread_create(&th, NULL, xfer_thread, task) != 0)
    {
        conn->xfer_in_progress = 0;
        free(task);
        socket_send(conn->ctrl_fd, "451 Local error: cannot start transfer.\r\n");
        return;
    }
    pthread_detach(th);
}

static void cmd_handle_cwd(ClientConn *conn, const char *args)
{
    if (conn->auth_state != AUTH_STATE_AUTHED)
    {
        socket_send(conn->ctrl_fd, "530 Please login with USER and PASS.\r\n");
        return;
    }
    if (!args || args[0] == '\0')
    {
        socket_send(conn->ctrl_fd, "501 Syntax error in parameters or arguments.\r\n");
        return;
    }

    // 解析目标路径合法性（不允许超出根目录）
    char target[PATH_MAX];
    if (resolve_abs_path(conn, args, target, sizeof(target)) != 0)
    {
        socket_send(conn->ctrl_fd, "550 Failed to change directory.\r\n");
        return;
    }

    // 检查目标路径是否为真实存在的目录
    struct stat st;
    if (stat(target, &st) != 0 || !S_ISDIR(st.st_mode))
    {
        socket_send(conn->ctrl_fd, "550 Failed to change directory.\r\n");
        return;
    }

    strncpy(conn->current_dir, target, sizeof(conn->current_dir) - 1);
    conn->current_dir[sizeof(conn->current_dir) - 1] = '\0';
    socket_send(conn->ctrl_fd, "250 Directory successfully changed.\r\n");
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
            socket_send(conn->ctrl_fd, "500 Internal error.\r\n");
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
        socket_send(conn->ctrl_fd, "530 Please login with USER anonymous.\r\n");
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
        socket_send(conn->ctrl_fd, "331 User accepted, send PASS (email address).\r\n");
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
    socket_send(conn->ctrl_fd, "502 Command not implemented.\r\n");
}

// 以下为内部命令处理函数（仅在.c中实现，.h不暴露）
// int cmd_handle_cwd(ClientConn* conn, const char* args);   // 处理CWD命令
// int cmd_handle_pwd(ClientConn* conn, const char* args);   // 处理PWD命令
// int cmd_handle_mkd(ClientConn* conn, const char* args);   // 处理MKD命令
// int cmd_handle_rmd(ClientConn* conn, const char* args);   // 处理RMD命令
// int cmd_handle_list(ClientConn* conn, const char* args);  // 处理LIST命令
