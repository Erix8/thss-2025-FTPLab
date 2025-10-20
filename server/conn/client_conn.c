#include "client_conn.h"

/**
 * 初始化客户端连接信息结构体
 * @param conn 客户端连接结构体指针
 * @param ctrl_fd 控制连接文件描述符
 * @param root_dir FTP服务器根目录路径
 */
void client_conn_init(ClientConn *conn, int ctrl_fd, const char *root_dir)
{
}

/**
 * 运行连接管理器，处理多客户端连接（基于select循环）
 * @param listen_fd 监听文件描述符
 * @param config 服务器配置结构体指针
 */
void conn_manager_run(int listen_fd, const ServerConfig *config)
{
}