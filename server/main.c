#include "config/config.h"
#include "net/socket_utils.h"
#include "conn/client_conn.h"

int main(int argc, char *argv[])
{
    ServerConfig config;
    // 初始化配置
    config_init(argc, argv, &config);
    if (config_validate(&config) != 0)
    {
        return 1;
    }
    // 创建监听socket
    int listen_fd = socket_create_listen(config.port);
    if (listen_fd == -1)
    {
        return 1;
    }
    // 启动连接管理器
    conn_manager_run(listen_fd, &config);
    // 关闭监听socket
    socket_close(listen_fd);
    return 0;
}