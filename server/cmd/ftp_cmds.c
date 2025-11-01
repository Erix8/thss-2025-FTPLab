#include "ftp_cmds.h"
#include "../net/socket_utils.h"
#include "../utils/utils.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <time.h>

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

    // 构造参数片段进行比较
    char param[8];
    size_t len = (size_t)(end - args);
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
    socket_send(conn->ctrl_fd, "502 Command not implemented.\r\n");
}

// 以下为内部命令处理函数（仅在.c中实现，.h不暴露）
// int cmd_handle_retr(ClientConn* conn, const char* args);  // 处理RETR命令
// int cmd_handle_stor(ClientConn* conn, const char* args);  // 处理STOR命令
// int cmd_handle_cwd(ClientConn* conn, const char* args);   // 处理CWD命令
// int cmd_handle_pwd(ClientConn* conn, const char* args);   // 处理PWD命令
// int cmd_handle_mkd(ClientConn* conn, const char* args);   // 处理MKD命令
// int cmd_handle_rmd(ClientConn* conn, const char* args);   // 处理RMD命令
// int cmd_handle_list(ClientConn* conn, const char* args);  // 处理LIST命令
