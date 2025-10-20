#ifndef CLIENT_TRANSFER_H
#define CLIENT_TRANSFER_H

// 用PORT模式建立数据连接（返回数据连接fd，失败返回-1）
int transfer_setup_port(int ctrl_fd, const char *local_ip);

// 用PASV模式建立数据连接（返回数据连接fd，失败返回-1）
int transfer_setup_pasv(int ctrl_fd, const char *server_ip);

// 从数据连接接收文件（保存到本地，返回0成功）
int transfer_recv_file(int data_fd, const char *local_filename);

// 向数据连接发送文件（上传本地文件，返回0成功）
int transfer_send_file(int data_fd, const char *local_filename);

// 关闭数据连接
void transfer_close_data_conn(int data_fd);

#endif