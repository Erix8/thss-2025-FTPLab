#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>         // getcwd
#include <limits.h>         // PATH_MAX
#include <sys/stat.h>       // stat, S_ISDIR
#include "../utils/utils.h" // utils_join_path

/**
 * 初始化服务器配置（解析命令行参数并设置默认值）
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * @param config 输出服务器配置结构体
 */
void config_init(int argc, char *argv[], ServerConfig *config)
{
    // 默认配置
    config->port = 21;
    snprintf(config->root_dir, sizeof(config->root_dir), "/tmp"); // 默认根目录/tmp
    config->max_conn = 20;                                        // 默认最大连接数

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
            const char *arg_path = argv[i + 1];
            char resolved[PATH_MAX];

            // 如果是绝对路径（以'/'开头），直接尝试规范化；否则与当前工作目录拼接
            if (arg_path[0] == '/')
            {
                // realpath 仅在路径存在时成功；若失败则退回原始路径
                if (realpath(arg_path, resolved) != NULL)
                {
                    snprintf(config->root_dir, sizeof(config->root_dir), "%s", resolved);
                }
                else
                {
                    snprintf(config->root_dir, sizeof(config->root_dir), "%s", arg_path);
                }
            }
            else
            {
                char cwd[PATH_MAX];
                if (getcwd(cwd, sizeof(cwd)) != NULL)
                {
                    char joined[PATH_MAX];
                    // 使用 utils_join_path 安全地拼接 CWD 和相对路径
                    if (utils_join_path(cwd, arg_path, joined, sizeof(joined)) != NULL)
                    {
                        // 优先尝试规范化为真实路径（若存在）
                        if (realpath(joined, resolved) != NULL)
                        {
                            snprintf(config->root_dir, sizeof(config->root_dir), "%s", resolved);
                        }
                        else
                        {
                            // realpath 失败（可能路径尚不存在），使用拼接结果
                            snprintf(config->root_dir, sizeof(config->root_dir), "%s", joined);
                        }
                    }
                    else
                    {
                        // 拼接失败，退回原始相对路径
                        snprintf(config->root_dir, sizeof(config->root_dir), "%s", arg_path);
                    }
                }
                else
                {
                    // 获取工作目录失败，退回原始参数
                    snprintf(config->root_dir, sizeof(config->root_dir), "%s", arg_path);
                }
            }

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
    // 校验根目录合法性：非空、存在、为目录、可读可进入
    if (config->root_dir[0] == '\0')
    {
        // fprintf(stderr, "Root dir is empty.\n");
        return 2;
    }

    struct stat st;
    if (stat(config->root_dir, &st) != 0)
    {
        // fprintf(stderr, "Root dir does not exist: %s\n", config->root_dir);
        return 2;
    }
    if (!S_ISDIR(st.st_mode))
    {
        // fprintf(stderr, "Root path is not a directory: %s\n", config->root_dir);
        return 3;
    }
    // 需要具备读和执行权限（进入目录）
    if (access(config->root_dir, R_OK | X_OK) != 0)
    {
        // fprintf(stderr, "Root dir is not accessible: %s\n", config->root_dir);
        return 4;
    }
    return 0;
}