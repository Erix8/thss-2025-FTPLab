#ifndef CLIENT_TRANSFER_H
#define CLIENT_TRANSFER_H

// 从数据连接接收文件（保存到本地，返回0成功）
int transfer_recv_file(int data_fd, const char *local_filename);

// 向数据连接发送文件（上传本地文件，返回0成功）
int transfer_send_file(int data_fd, const char *local_filename);

// 关闭数据连接
void transfer_close_data_conn(int data_fd);

// 接收目录列表（LIST/NLST），可选将 CRLF 规范化为 \n
// - data_fd: 数据连接 fd
// - local_filename: 若为 NULL 则输出到标准输出；否则写入指定文件
// - normalize_crlf: 非0则将 "\r\n" 转为 "\n"，并处理孤立的 '\r'
// 返回0成功，非0失败
int transfer_recv_list(int data_fd, const char *local_filename, int normalize_crlf);

#endif