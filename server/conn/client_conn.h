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
    int ctrl_fd;                   // 控制连接socket（-1表示空闲）
    int data_fd;                   // 数据连接socket
    AuthState auth_state;          // 认证状态
    char current_dir[PATH_MAX];    // 当前工作目录（基于根目录）
    DataMode data_mode;            // 数据连接模式
    char peer_ip[INET_ADDRSTRLEN]; // 客户端IP（PORT模式用）
    int peer_port;                 // 客户端端口（PORT模式用）
    int pasv_port;                 // 服务器临时端口（PASV模式用）
    int pending_user_anon;         // 0: 未进入匿名登录流程；1: 已收到USER anonymous，等待PASS
} ClientConn;

typedef struct
{
    ClientConn *data; // 动态数组指针（存储客户端连接）
    size_t capacity;  // 当前数组容量（最大可存储连接数）
    size_t used;      // 已使用连接数（活跃客户端数）
} ClientConnList;

// 初始化客户端连接（设置默认状态：未认证、根目录为初始目录）
void client_conn_init(ClientConn *conn, int ctrl_fd, const char *root_dir);

// 初始化客户端连接列表
ClientConnList *client_conn_list_init(size_t init_capacity);

// 向动态列表添加新客户端连接
int client_conn_list_add(ClientConnList *list, ClientConn *new_conn);

// 从动态列表移除指定fd的客户端连接
void client_conn_list_remove(ClientConnList *list, int ctrl_fd);

// 销毁动态列表及所有连接资源
void client_conn_list_destroy(ClientConnList *list);

// 处理单个客户端请求：返回1表示需要关闭并移除该连接，0表示保留连接
int handle_client_cmd(ClientConn *conn);

// 管理多客户端连接（基于select()循环，处理就绪事件）
void conn_manager_run(int listen_fd, const ServerConfig *config);

#endif