#include "ui_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * 读取用户输入的命令字符串
 * @return 动态分配的输入字符串（需调用者用free释放），用户退出时返回NULL
 */
char *ui_read_input()
{
    char buf[1024];
    if (fgets(buf, sizeof(buf), stdin) == NULL)
    {
        return NULL; // 用户输入Ctrl+D
    }
    // 移除末尾的换行符
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n')
    {
        buf[len - 1] = '\0';
    }
    // 动态分配内存返回
    char *input = strdup(buf);
    return input;
}

/**
 * 打印服务器响应信息或操作结果提示
 * @param msg 要打印的信息字符串
 */
void ui_print_msg(const char *msg)
{
    if (!msg)
        return;
    // 去掉结尾的 \r 和 \n（不修改原字符串）
    size_t len = strlen(msg);
    while ((len > 0) && ((msg[len - 1] == '\n') || (msg[len - 1] == '\r')))
        len--;
    // 按截断后的长度打印，并追加一个换行，保持一行一条
    printf("%.*s\n", (int)len, msg);
    fflush(stdout);
}

/**
 * 解析命令行参数：支持 -ip IPaddress 与 -port n；未指定使用默认
 * 成功返回0，失败返回非0并输出用法
 */
int ui_parse_args(int argc, char *argv[], char *ip_out, size_t ip_len, int *port_out)
{
    if (!ip_out || ip_len == 0 || !port_out)
        return -1;

    // 默认值
    snprintf(ip_out, ip_len, "%s", "127.0.0.1");
    *port_out = 21;

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "-ip") == 0)
        {
            if (i + 1 >= argc)
            {
                // ui_print_msg("Usage: ./client [-ip IPaddress] [-port n]");
                return -1;
            }
            snprintf(ip_out, ip_len, "%s", argv[i + 1]);
            i++;
        }
        else if (strcmp(argv[i], "-port") == 0)
        {
            if (i + 1 >= argc)
            {
                // ui_print_msg("Usage: ./client [-ip IPaddress] [-port n]");
                return -1;
            }
            char *endp = NULL;
            long p = strtol(argv[i + 1], &endp, 10);
            if (endp && *endp != '\0')
            {
                // ui_print_msg("Usage: ./client [-ip IPaddress] [-port n]");
                return -1;
            }
            if (p <= 0 || p > 65535)
            {
                // ui_print_msg("Invalid port: must be 1-65535");
                return -1;
            }
            *port_out = (int)p;
            i++;
        }
        else
        {
            // ui_print_msg("Usage: ./client [-ip IPaddress] [-port n]");
            return -1;
        }
    }

    return 0;
}