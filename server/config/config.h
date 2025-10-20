#ifndef CONFIG_H
#define CONFIG_H

#include <limits.h>
#include <unistd.h>
// 服务器全局配置
typedef struct
{
    int port;                // 监听端口（默认2121）
    char root_dir[PATH_MAX]; // FTP根目录（默认"./ftp_root"）
    int max_conn;            // 最大连接数（默认10）
} ServerConfig;

// 初始化配置（解析命令行参数，设置默认值）
void config_init(int argc, char *argv[], ServerConfig *config);

// 校验配置合法性（根目录是否存在、端口是否有效）
int config_validate(ServerConfig *config);

#endif