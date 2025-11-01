#include "ftp_cmds.h"
#include "../net/socket_utils.h"
#include "../utils/utils.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

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
    socket_send(conn->ctrl_fd, "502 Command not implemented.\r\n");
}

// 以下为内部命令处理函数（仅在.c中实现，.h不暴露）
// int cmd_handle_port(ClientConn* conn, const char* args);  // 处理PORT命令
// int cmd_handle_pasv(ClientConn* conn, const char* args);  // 处理PASV命令
// int cmd_handle_retr(ClientConn* conn, const char* args);  // 处理RETR命令
// int cmd_handle_stor(ClientConn* conn, const char* args);  // 处理STOR命令
// int cmd_handle_cwd(ClientConn* conn, const char* args);   // 处理CWD命令
// int cmd_handle_pwd(ClientConn* conn, const char* args);   // 处理PWD命令
// int cmd_handle_mkd(ClientConn* conn, const char* args);   // 处理MKD命令
// int cmd_handle_rmd(ClientConn* conn, const char* args);   // 处理RMD命令
// int cmd_handle_list(ClientConn* conn, const char* args);  // 处理LIST命令
// int cmd_handle_syst(ClientConn* conn, const char* args);  // 处理SYST命令
// int cmd_handle_type(ClientConn* conn, const char* args);  // 处理TYPE命令
// int cmd_handle_quit(ClientConn* conn, const char* args);  // 处理QUIT命令