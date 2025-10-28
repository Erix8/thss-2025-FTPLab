#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/**
 * 初始化服务器配置（解析命令行参数并设置默认值）
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * @param config 输出服务器配置结构体
 */
void config_init(int argc, char *argv[], ServerConfig *config)
{
    // 默认配置
    config->port = 21;                // 默认端口21
    strcpy(config->root_dir, "/tmp"); // 默认根目录
    config->max_conn = 20;            // 默认最大连接数

    // 简单解析命令行参数（仅支持 -p 端口）
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc)
        {
            config->port = atoi(argv[i + 1]);
            i++;
        }
    }
}

/**
 * 校验服务器配置的合法性
 * @param config 服务器配置结构体指针
 * @return 0表示配置合法，非0表示配置非法
 */
int config_validate(ServerConfig *config)
{
    // 校验端口范围
    if (config->port < 1 || config->port > 65535)
    {
        fprintf(stderr, "Invalid port: %d\n", config->port);
        return 1;
    }
    // 其他校验（如根目录存在性）暂略
    return 0;
}