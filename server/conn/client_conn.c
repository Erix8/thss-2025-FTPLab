#include "client_conn.h"
#include "../net/socket_utils.h"
#include "../../utils/utils.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/**
 * 初始化客户端连接信息结构体
 * @param conn 客户端连接结构体指针
 * @param ctrl_fd 控制连接文件描述符
 * @param root_dir FTP服务器根目录路径
 */
void client_conn_init(ClientConn *conn, int ctrl_fd, const char *root_dir)
{
    memset(conn, 0, sizeof(ClientConn));
    conn->ctrl_fd = ctrl_fd;
    conn->auth_state = AUTH_STATE_UNAUTH;
    strcpy(conn->current_dir, root_dir); // 初始目录为根目录
}

// 处理单个客户端请求（简化版，仅支持USER/QUIT命令）
void handle_client(ClientConn *conn)
{
    // 发送欢迎信息
    socket_send(conn->ctrl_fd, "220 Welcome to simple FTP server");

    char buf[1024];
    while (1)
    {
        // 接收客户端命令
        ssize_t n = socket_recv(conn->ctrl_fd, buf, sizeof(buf));
        if (n <= 0)
            break;

        // 分割命令和参数
        char cmd[16], args[1024];
        utils_split_cmd(buf, cmd, sizeof(cmd), args, sizeof(args));

        // 处理命令
        if (strcmp(cmd, "USER") == 0)
        {
            // 简单认证：只要提供用户名就通过
            socket_send(conn->ctrl_fd, "230 Login successful");
            conn->auth_state = AUTH_STATE_AUTHED;
        }
        else if (strcmp(cmd, "QUIT") == 0)
        {
            socket_send(conn->ctrl_fd, "221 Goodbye");
            break;
        }
        else
        {
            socket_send(conn->ctrl_fd, "502 Command not implemented");
        }
    }

    // 关闭连接
    socket_close(conn->ctrl_fd);
}

/**
 * 运行连接管理器，处理多客户端连接（基于select循环）
 * @param listen_fd 监听文件描述符
 * @param config 服务器配置结构体指针
 */
void conn_manager_run(int listen_fd, const ServerConfig *config)
{
    while (1)
    {
        char client_ip[INET_ADDRSTRLEN];
        int client_port;
        int ctrl_fd = socket_accept(listen_fd, client_ip, &client_port);
        if (ctrl_fd == -1)
            continue;

        printf("New connection from %s:%d\n", client_ip, client_port);

        // 初始化客户端连接并处理
        ClientConn conn;
        client_conn_init(&conn, ctrl_fd, config->root_dir);
        handle_client(&conn);

        printf("Connection closed\n");
    }
}