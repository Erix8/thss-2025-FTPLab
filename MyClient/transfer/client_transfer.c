#include "client_transfer.h"

/**
 * 用PORT模式建立数据连接
 * @param ctrl_fd 控制连接文件描述符
 * @param local_ip 本地IP地址（用于告知服务器）
 * @return 成功返回数据连接文件描述符，失败返回-1
 */
int transfer_setup_port(int ctrl_fd, const char *local_ip);

/**
 * 用PASV模式建立数据连接
 * @param ctrl_fd 控制连接文件描述符
 * @param server_ip 服务器IP地址
 * @return 成功返回数据连接文件描述符，失败返回-1
 */
int transfer_setup_pasv(int ctrl_fd, const char *server_ip);

/**
 * 从数据连接接收文件并保存到本地
 * @param data_fd 数据连接文件描述符
 * @param local_filename 本地保存的文件名
 * @return 0表示成功，非0表示失败
 */
int transfer_recv_file(int data_fd, const char *local_filename);

/**
 * 向数据连接发送本地文件（上传）
 * @param data_fd 数据连接文件描述符
 * @param local_filename 本地要上传的文件名
 * @return 0表示成功，非0表示失败
 */
int transfer_send_file(int data_fd, const char *local_filename);

/**
 * 关闭数据连接
 * @param data_fd 要关闭的数据连接文件描述符
 */
void transfer_close_data_conn(int data_fd);