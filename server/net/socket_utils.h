#ifndef SOCKET_UTILS_H
#define SOCKET_UTILS_H

#include <sys/socket.h>

// 创建并监听控制连接socket（返回监听fd，失败返回-1）
int socket_create_listen(int port);

// 接受客户端连接（返回控制连接fd，失败返回-1）
int socket_accept(int listen_fd, char *client_ip, int *client_port);

// 发送数据（自动添加FTP要求的\r\n结尾，返回发送字节数）
ssize_t socket_send(int fd, const char *data);

// 接收数据（读取到\r\n或缓冲区满，返回接收字节数）
ssize_t socket_recv(int fd, char *buf, size_t buf_len);

// 关闭socket并释放资源
void socket_close(int fd);

#endif