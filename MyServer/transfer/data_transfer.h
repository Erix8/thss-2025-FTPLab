#ifndef DATA_TRANSFER_H
#define DATA_TRANSFER_H

#include "../conn/client_conn.h"

// 建立数据连接（根据ClientConn的data_mode，返回0成功，-1失败）
int transfer_init_data_conn(ClientConn *conn);

// 发送文件到客户端（通过数据连接，返回0成功，-1失败）
int transfer_send_file(ClientConn *conn, const char *filename);

// 接收客户端上传的文件（通过数据连接，返回0成功，-1失败）
int transfer_recv_file(ClientConn *conn, const char *filename);

// 发送目录列表到客户端（通过数据连接，返回0成功，-1失败）
int transfer_send_list(ClientConn *conn);

// 关闭数据连接
void transfer_close_data_conn(ClientConn *conn);

#endif