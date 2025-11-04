#include "client_transfer.h"
#include "../net/client_socket.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

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