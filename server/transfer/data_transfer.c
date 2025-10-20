#include "data_transfer.h"

/**
 * 初始化数据连接（根据客户端连接的data_mode）
 * @param conn 客户端连接结构体指针
 * @return 0表示成功，-1表示失败
 */
int transfer_init_data_conn(ClientConn *conn);

/**
 * 通过数据连接向客户端发送文件
 * @param conn 客户端连接结构体指针
 * @param filename 要发送的文件名（基于服务器当前目录）
 * @return 0表示成功，-1表示失败
 */
int transfer_send_file(ClientConn *conn, const char *filename);

/**
 * 通过数据连接接收客户端上传的文件
 * @param conn 客户端连接结构体指针
 * @param filename 要保存的文件名（基于服务器当前目录）
 * @return 0表示成功，-1表示失败
 */
int transfer_recv_file(ClientConn *conn, const char *filename);

/**
 * 通过数据连接向客户端发送目录列表
 * @param conn 客户端连接结构体指针
 * @return 0表示成功，-1表示失败
 */
int transfer_send_list(ClientConn *conn);

/**
 * 关闭客户端连接的数据连接
 * @param conn 客户端连接结构体指针
 */
void transfer_close_data_conn(ClientConn *conn);