#include "socket_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

/**
 * 创建并监听FTP服务器控制连接端口
 * @param port 要监听的端口号
 * @return 成功返回监听文件描述符，失败返回-1
 */
int socket_create_listen(int port)
{
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1)
    {
        perror("socket create failed");
        return -1;
    }

    // 设置端口复用
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 绑定地址
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        perror("bind failed");
        close(listen_fd);
        return -1;
    }

    // 开始监听
    if (listen(listen_fd, 5) == -1)
    {
        perror("listen failed");
        close(listen_fd);
        return -1;
    }

    printf("Server listening on port %d\n", port);
    return listen_fd;
}

/**
 * 接受客户端的控制连接请求
 * @param listen_fd 监听文件描述符
 * @param client_ip 输出客户端IP地址的缓冲区
 * @param client_port 输出客户端端口号的指针
 * @return 成功返回控制连接文件描述符，失败返回-1
 */
int socket_accept(int listen_fd, char *client_ip, int *client_port)
{
    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);
    int ctrl_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &len);
    if (ctrl_fd == -1)
    {
        perror("accept failed");
        return -1;
    }

    // 保存客户端信息
    if (client_ip)
        strcpy(client_ip, inet_ntoa(client_addr.sin_addr));
    if (client_port)
        *client_port = ntohs(client_addr.sin_port);
    return ctrl_fd;
}

/**
 * 向指定文件描述符发送数据（自动添加\r\n结尾）
 * @param fd 目标文件描述符
 * @param data 要发送的数据字符串
 * @return 成功返回发送的字节数，失败返回-1
 */
ssize_t socket_send(int fd, const char *data)
{
    if (!data)
        return -1;
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s\r\n", data); // 添加FTP换行符
    return send(fd, buf, strlen(buf), 0);
}

/**
 * 从指定文件描述符接收数据（直到\r\n或缓冲区满）
 * @param fd 源文件描述符
 * @param buf 存储接收数据的缓冲区
 * @param buf_len 缓冲区长度
 * @return 成功返回接收的字节数，失败返回-1
 */
ssize_t socket_recv(int fd, char *buf, size_t buf_len)
{
    if (!buf || buf_len == 0)
        return -1;
    ssize_t n = recv(fd, buf, buf_len - 1, 0); // 留一个字节给终止符
    if (n <= 0)
        return n;

    buf[n] = '\0';
    // 移除末尾的\r\n
    if (n >= 2 && buf[n - 2] == '\r' && buf[n - 1] == '\n')
    {
        buf[n - 2] = '\0';
    }
    return n;
}

/**
 * 关闭socket文件描述符并释放相关资源
 * @param fd 要关闭的socket文件描述符
 */
void socket_close(int fd)
{
    close(fd);
}
