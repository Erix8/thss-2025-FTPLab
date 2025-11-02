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
    config->port = 21;                                               // 默认端口21
    strncpy(config->root_dir, "/tmp", sizeof(config->root_dir) - 1); // 默认根目录/tmp
    config->root_dir[sizeof(config->root_dir) - 1] = '\0';           // 确保字符串以'\0'结尾
    config->max_conn = 20;                                           // 默认最大连接数

    // 简单解析命令行参数（仅支持 -p 端口）
    for (int i = 1; i < argc; i++)
    {
        if ((strcmp(argv[i], "-port") == 0) && i + 1 < argc)
        {
            char *endp = NULL;
            long p = strtol(argv[i + 1], &endp, 10);
            if (endp && *endp == '\0' && p >= 1 && p <= 65535)
            {
                config->port = (int)p;
            }
            i++; // 跳过端口参数
        }
        else if (strcmp(argv[i], "-root") == 0 && i + 1 < argc)
        {
            strncpy(config->root_dir, argv[i + 1], sizeof(config->root_dir) - 1);
            config->root_dir[sizeof(config->root_dir) - 1] = '\0';
            i++; // 跳过路径参数
        }
        // 其他参数忽略
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
        // fprintf(stderr, "Invalid port: %d\n", config->port);
        return 1;
    }
    // 其他校验（如根目录存在性）暂略
    return 0;
}