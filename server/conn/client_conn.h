#ifndef CLIENT_CONN_H
#define CLIENT_CONN_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <limits.h>
#include <unistd.h>
#include "../config/config.h"

// 认证状态
typedef enum
{
    AUTH_STATE_UNAUTH, // 未认证
    AUTH_STATE_AUTHED  // 已认证
} AuthState;

// 数据连接模式
typedef enum
{
    DATA_MODE_PORT, // PORT模式（主动）
    DATA_MODE_PASV  // PASV模式（被动）
} DataMode;

// 客户端连接信息
typedef struct
{
    int ctrl_fd;                   // 控制连接socket
    int data_fd;                   // 数据连接socket（临时）
    AuthState auth_state;          // 认证状态
    char current_dir[PATH_MAX];    // 当前工作目录（基于根目录）
    DataMode data_mode;            // 数据连接模式
    char peer_ip[INET_ADDRSTRLEN]; // 客户端IP（PORT模式用）
    int peer_port;                 // 客户端端口（PORT模式用）
    int pasv_port;                 // 服务器临时端口（PASV模式用）
} ClientConn;

// 初始化客户端连接（设置默认状态：未认证、根目录为初始目录）
void client_conn_init(ClientConn *conn, int ctrl_fd, const char *root_dir);

// 管理多客户端连接（基于select()循环，处理就绪事件）
void conn_manager_run(int listen_fd, const ServerConfig *config);

#endif