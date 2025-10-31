#include "ftp_cmds.h"
#include "../net/socket_utils.h"
#include <string.h>

static void cmd_handle_user(ClientConn *conn, const char *args)
{
    // 检查是否已认证
    if (conn->auth_state == AUTH_STATE_AUTHED)
    {
        socket_send(conn->ctrl_fd, "503 Already logged in");
        return;
    }

    // 验证用户名是否为anonymous
    if (args == NULL || strcmp(args, "anonymous") != 0)
    {
        socket_send(conn->ctrl_fd, "530 Only anonymous login supported");
        return;
    }

    // 接受匿名用户，提示输入密码(邮箱)
    socket_send(conn->ctrl_fd, "331 Please specify the password (email address)");
    return;
}

static void cmd_handle_pass(ClientConn *conn, const char *args)
{
    // 检查认证状态
    if (conn->auth_state == AUTH_STATE_AUTHED)
    {
        socket_send(conn->ctrl_fd, "503 Already logged in");
        return;
    }

    // 简单验证密码不为空(实际匿名登录通常不严格验证邮箱格式)
    if (args == NULL || strlen(args) == 0)
    {
        socket_send(conn->ctrl_fd, "501 Password required (email address)");
        return;
    }

    // 标记为已认证并发送成功消息
    conn->auth_state = AUTH_STATE_AUTHED;
    socket_send(conn->ctrl_fd, "230 Login successful");
    return;
}

/**
 * 处理客户端发送的命令行
 * @param conn 客户端连接信息结构体指针
 * @param cmd 客户端发送的命令字符串
 * @param args 客户端发送的命令参数字符串
 */
void cmd_process(ClientConn *conn, const char *cmd, const char *args)
{
    if (strcmp(cmd, "USER") == 0)
    {
        cmd_handle_user(conn, args);
    }
    else if (strcmp(cmd, "PASS") == 0)
    {
        cmd_handle_pass(conn, args);
    }
    else
    {
        socket_send(conn->ctrl_fd, "502 Command not implemented");
    }
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