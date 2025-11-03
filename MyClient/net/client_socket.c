#include "client_socket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
/**
 * 连接到FTP服务器并建立控制连接
 * @param server_ip 服务器IP地址字符串
 * @param server_port 服务器端口号
 * @return 成功返回控制连接文件描述符，失败返回-1
 */
int client_connect(const char *server_ip, int server_port)
{
    int ctrl_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (ctrl_fd == -1)
    {
        perror("socket create failed");
        return -1;
    }

    // 连接服务器
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0)
    {
        perror("invalid server ip");
        close(ctrl_fd);
        return -1;
    }

    if (connect(ctrl_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("connect failed");
        close(ctrl_fd);
        return -1;
    }

    return ctrl_fd;
}

/**
 * 向服务器发送FTP命令（自动添加\r\n结尾）
 * @param ctrl_fd 控制连接文件描述符
 * @param cmd 要发送的命令字符串（如"USER anonymous"）
 * @return 成功返回发送的字节数，失败返回-1
 */
ssize_t client_send_cmd(int ctrl_fd, const char *cmd)
{
    if (!cmd)
        return -1;
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s\r\n", cmd); // 添加FTP换行符
    return send(ctrl_fd, buf, strlen(buf), 0);
}

/**
 * 接收服务器的响应信息
 * @param ctrl_fd 控制连接文件描述符
 * @param resp_buf 存储响应信息的缓冲区
 * @param buf_len 缓冲区长度
 * @return 响应码的前3位（如220、550），失败返回-1
 */
int client_recv_resp(int ctrl_fd, char *resp_buf, size_t buf_len)
{
    if (!resp_buf || buf_len == 0)
        return -1;
    ssize_t n = recv(ctrl_fd, resp_buf, buf_len - 1, 0);
    if (n <= 0)
        return -1;

    resp_buf[n] = '\0';
    // 提取响应码（前3位数字）
    int code = atoi(resp_buf);
    return code;
}

/**
 * 关闭与服务器的控制连接
 * @param ctrl_fd 控制连接文件描述符
 */
void client_disconnect(int ctrl_fd)
{
    close(ctrl_fd);
}