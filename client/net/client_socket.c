#include "client_socket.h"

/**
 * 连接到FTP服务器并建立控制连接
 * @param server_ip 服务器IP地址字符串
 * @param server_port 服务器端口号
 * @return 成功返回控制连接文件描述符，失败返回-1
 */
int client_connect(const char *server_ip, int server_port);

/**
 * 向服务器发送FTP命令（自动添加\r\n结尾）
 * @param ctrl_fd 控制连接文件描述符
 * @param cmd 要发送的命令字符串（如"USER anonymous"）
 * @return 成功返回发送的字节数，失败返回-1
 */
ssize_t client_send_cmd(int ctrl_fd, const char *cmd);

/**
 * 接收服务器的响应信息
 * @param ctrl_fd 控制连接文件描述符
 * @param resp_buf 存储响应信息的缓冲区
 * @param buf_len 缓冲区长度
 * @return 响应码的前3位（如220、550），失败返回-1
 */
int client_recv_resp(int ctrl_fd, char *resp_buf, size_t buf_len);

/**
 * 关闭与服务器的控制连接
 * @param ctrl_fd 控制连接文件描述符
 */
void client_disconnect(int ctrl_fd);