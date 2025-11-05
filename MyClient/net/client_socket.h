#ifndef CLIENT_SOCKET_H
#define CLIENT_SOCKET_H

#include <stddef.h>
#include <stdint.h>
#include <sys/socket.h>

// 连接FTP服务器（返回控制连接fd，失败返回-1）
int client_connect(const char *server_ip, int server_port);

// 发送FTP命令（自动添加\r\n，返回发送字节数）
ssize_t client_send_cmd(int ctrl_fd, const char *cmd);

// 接收服务器响应（读取到\r\n，返回响应码前3位，如220/550）
int client_recv_resp(int ctrl_fd, char *resp_buf, size_t buf_len);

// 关闭控制连接
void client_disconnect(int ctrl_fd);

// 在本地指定 IP:port 上创建监听 socket（用于主动 PORT 模式）
// 返回监听 socket 的 fd（>=0），失败返回 -1
int client_listen_port(const char *local_ip, uint16_t port);

#endif