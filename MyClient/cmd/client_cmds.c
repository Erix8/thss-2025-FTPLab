#include "client_cmds.h"
#include "../net/client_socket.h"
#include "../transfer/client_transfer.h"
#include "../ui/ui_utils.h"
#include "../utils/utils.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <errno.h>

/**
 * 初始化客户端结构体
 * @param client 指向Client结构体的指针
 */
void client_init(Client *client)
{
    if (client)
    {
        client->ctrl_fd = -1;
        client->data_fd = -1;
        client->state = CLIENT_STATE_UNAUTH;
        client->data_mode = DATA_MODE_NONE;
    }
}
/**
 * 处理PASV命令, 进入被动模式,
 * 保存服务器传送来的数据连接信息，存储在data_fd中
 * @param client 指向Client结构体的指针
 * @param args 命令参数字符串
 * @return 0表示成功，非0表示失败
 */
static void client_handle_pasv(Client *client)
{
    if (!client)
        return;

    char resp[8192];
    if (client_recv_resp(client->ctrl_fd, resp, sizeof(resp)) < 0)
    {
        ui_print_msg("Failed to receive PASV response.");
        return;
    }
    ui_print_msg(resp);

    // 解析PASV响应，提取IP和端口
    const char *p = strchr(resp, '(');
    const char *q = strchr(resp, ')');
    if (!p || !q || p >= q)
    {
        ui_print_msg("Invalid PASV response format.");
        return;
    }

    int h1, h2, h3, h4, p1, p2;
    if (sscanf(p + 1, "%d,%d,%d,%d,%d,%d", &h1, &h2, &h3, &h4, &p1, &p2) != 6)
    {
        ui_print_msg("Failed to parse PASV response.");
        return;
    }

    char ip[64];
    snprintf(ip, sizeof(ip), "%d.%d.%d.%d", h1, h2, h3, h4);
    uint16_t port = (uint16_t)(p1 * 256 + p2);

    // 建立数据连接
    int data_fd = client_connect(ip, port);
    if (data_fd == -1)
    {
        ui_print_msg("Failed to connect to PASV data port.");
        return;
    }

    client->data_mode = DATA_MODE_PASV;
    client->data_fd = data_fd;
}

/**
 * 处理PORT命令, 进入主动模式,
 * 在本地指定IP:port上监听，等待服务器连接回连
 * @param client 指向Client结构体的指针
 * @param args 命令参数字符串
 */
static void client_handle_port(Client *client, char *args)
{
    if (!client || !args)
        return;

    char resp[8192];
    if (client_recv_resp(client->ctrl_fd, resp, sizeof(resp)) < 0)
    {
        ui_print_msg("Failed to receive PORT response.");
        return;
    }
    ui_print_msg(resp);

    // 解析PORT命令参数
    int h1, h2, h3, h4, p1, p2;
    if (sscanf(args, "%d,%d,%d,%d,%d,%d", &h1, &h2, &h3, &h4, &p1, &p2) != 6)
        return;
    char ip[64];
    snprintf(ip, sizeof(ip), "%d.%d.%d.%d", h1, h2, h3, h4);
    uint16_t port = (uint16_t)(p1 * 256 + p2);

    // 在主动模式（PORT）中，客户端应在本地指定的IP:port上监听，等待服务器连接回连。
    int lfd = client_listen_port(ip, port);
    if (lfd < 0)
    {
        ui_print_msg("Failed to create listening socket for PORT mode.");
        return;
    }

    client->data_mode = DATA_MODE_PORT;
    client->data_fd = lfd; // 保存监听fd，后续数据传输时需 accept
}

static void client_handle_retr(Client *client, const char *args)
{
    if (!client || !args)
        return;

    char resp[8192];
    if (client_recv_resp(client->ctrl_fd, resp, sizeof(resp)) < 0)
    {
        ui_print_msg("Failed to receive RETR response.");
        return;
    }
    ui_print_msg(resp);

    // 根据数据连接模式，建立数据连接
    int data_fd = -1;
    if (client->data_mode == DATA_MODE_PASV)
    {
        data_fd = client->data_fd; // 已经在PASV处理中建立
    }
    else if (client->data_mode == DATA_MODE_PORT)
    {
        // 在PORT模式下，接受服务器的连接
        struct sockaddr_in server_addr;
        socklen_t addr_len = sizeof(server_addr);
        data_fd = accept(client->data_fd, (struct sockaddr *)&server_addr, &addr_len);
        if (data_fd < 0)
        {
            ui_print_msg("Failed to accept data connection in PORT mode.");
            return;
        }
    }
    else
    {
        ui_print_msg("Data connection mode not set.");
        return;
    }

    char *filename;
    if (strrchr(args, '/'))
    {
        filename = strrchr(args, '/');
        filename++; // 跳过最后的'/'
    }
    else
        filename = (char *)args;

    // 接收文件
    if (transfer_recv_file(data_fd, filename) != 0)
    {
        ui_print_msg("Failed to receive file.");
    }

    memset(resp, 0, sizeof(resp));
    if (client_recv_resp(client->ctrl_fd, resp, sizeof(resp)) < 0)
    {
        ui_print_msg("Failed to receive final response.");
    }
    ui_print_msg(resp);

    // 关闭数据连接
    transfer_close_data_conn(data_fd);
    client->data_fd = -1;
    client->data_mode = DATA_MODE_NONE;
}

static void client_handle_list(Client *client)
{
    if (!client)
        return;

    char resp[8192];
    if (client_recv_resp(client->ctrl_fd, resp, sizeof(resp)) < 0)
    {
        ui_print_msg("Failed to receive response.");
        return;
    }
    ui_print_msg(resp);

    // 根据数据连接模式，建立数据连接
    int data_fd = -1;
    if (client->data_mode == DATA_MODE_PASV)
    {
        data_fd = client->data_fd; // 已经在PASV处理中建立
    }
    else if (client->data_mode == DATA_MODE_PORT)
    {
        // 在PORT模式下，接受服务器的连接
        struct sockaddr_in server_addr;
        socklen_t addr_len = sizeof(server_addr);
        data_fd = accept(client->data_fd, (struct sockaddr *)&server_addr, &addr_len);
        if (data_fd < 0)
        {
            ui_print_msg("Failed to accept data connection in PORT mode.");
            return;
        }
    }
    else
    {
        ui_print_msg("Data connection mode not set.");
        return;
    }

    // 接收目录列表
    if (transfer_recv_list(data_fd, NULL, 1) != 0)
    {
        ui_print_msg("Failed to receive directory listing.");
    }

    memset(resp, 0, sizeof(resp));
    if (client_recv_resp(client->ctrl_fd, resp, sizeof(resp)) < 0)
    {
        ui_print_msg("Failed to receive final response.");
    }
    ui_print_msg(resp);

    // 关闭数据连接
    transfer_close_data_conn(data_fd);
    client->data_fd = -1;
    client->data_mode = DATA_MODE_NONE;
}

/**
 * 处理用户输入的客户端命令（转换为FTP协议命令）
 * @param ctrl_fd 控制连接文件描述符
 * @param state 客户端当前认证状态指针（可能被更新）
 * @param input 用户输入的命令字符串
 * @return 0表示处理成功，1表示退出，-1表示错误
 */
int client_handle_input(Client *client, const char *input)
{
    if (!input || !client)
        return -1;

    char cmd[16], args[1024];
    if (utils_split_cmd(input, cmd, sizeof(cmd), args, sizeof(args)) != 0)
    {
        return 0;
    }

    client_send_cmd(client->ctrl_fd, input);

    if (strcmp(cmd, "PASV") == 0)
    {
        client_handle_pasv(client);
        return 0;
    }
    else if (strcmp(cmd, "PORT") == 0)
    {
        client_handle_port(client, args);
        return 0;
    }
    else if (strcmp(cmd, "RETR") == 0)
    {
        client_handle_retr(client, args);
        return 0;
    }
    else if (strcmp(cmd, "STOR") == 0)
    {
        return 0;
    }
    else if (strcmp(cmd, "LIST") == 0)
    {
        client_handle_list(client);
        return 0;
    }
    else
    {
        // 其他一般指令
        char resp[8192];
        if (client_recv_resp(client->ctrl_fd, resp, sizeof(resp)) < 0)
        {
            ui_print_msg("Failed to receive response.");
            return -1;
        }
        ui_print_msg(resp);
        if (strcmp(cmd, "QUIT") == 0)
            return 1; // 退出标志
        return 0;
    }
}