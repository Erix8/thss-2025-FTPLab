#include "utils.h"
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <ctype.h>
#define CMD_MAX_LEN 16
#define ARGS_MAX_LEN 10

// 解析 "h1,h2,h3,h4,p1,p2" -> ip字符串和端口
int parse_port_arg(const char *args, char *ip_buf, size_t ip_len, uint16_t *port_out)
{
    if (!args || !ip_buf || !port_out)
        return -1;

    // 拷贝一份用于分割
    char tmp[128];
    strncpy(tmp, args, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    int parts[6] = {0};
    char *saveptr = NULL;
    char *tok = strtok_r(tmp, ",", &saveptr);
    for (int i = 0; i < 6; i++)
    {
        if (!tok)
            return -1;
        char *endp = NULL;
        long v = strtol(tok, &endp, 10);
        if (*endp != '\0' || v < 0 || v > 255)
            return -1;
        parts[i] = (int)v;
        tok = strtok_r(NULL, ",", &saveptr);
    }
    if (tok != NULL)
        return -1; // 多余字段

    // 组合 IP 和端口
    snprintf(ip_buf, ip_len, "%d.%d.%d.%d", parts[0], parts[1], parts[2], parts[3]);
    int p = parts[4] * 256 + parts[5];
    if (p <= 0 || p > 65535)
        return -1;
    *port_out = (uint16_t)p;
    return 0;
}

/**
 * 分割命令行字符串为命令和参数两部分
 * @param cmd_line 完整命令行（如"RETR file.txt"）
 * @param cmd 输出命令缓冲区（至少16字节）
 * @param cmd_len 输出命令缓冲区长度
 * @param args 输出参数缓冲区（至少1024字节）
 * @param args_len 输出参数缓冲区长度
 * @return 0：成功；-1：参数无效或缓冲区不足；-2：空命令
 */
int utils_split_cmd(const char *cmd_line, char *cmd, size_t cmd_len, char *args, size_t args_len)
{

    // 检验参数是否合格
    if (cmd_line == NULL || cmd == NULL || args == NULL)
    {
        return -1; // 无效指针
    }
    if (cmd_len < CMD_MAX_LEN || args_len < ARGS_MAX_LEN)
    {
        return -1; // 缓冲区大小不足
    }

    // 初始化输出缓冲区
    memset(cmd, 0, cmd_len);
    memset(args, 0, args_len);

    // 跳过前导空格
    const char *p = cmd_line;
    while (*p != '\0' && isspace((unsigned char)*p))
        p++;
    if (*p == '\0')
        return -2; // 空命令

    // 提取命令（到第一个空格为止）
    const char *cmd_start = p;
    while (*p != ' ' && *p != '\0')
        p++;
    cmd_len = p - cmd_start;

    // 命令转为大写并复制（限制最大长度15，留一个字节给终止符）
    if (cmd_len > CMD_MAX_LEN - 1)
        cmd_len = CMD_MAX_LEN - 1;

    for (size_t i = 0; i < cmd_len; i++)
    {
        unsigned char c = (unsigned char)cmd_start[i];
        cmd[i] = (char)toupper(c);
    }
    cmd[cmd_len] = '\0';

    // 提取参数（剩余部分，跳过中间空格）
    while (*p == ' ')
        p++;
    // 限制参数最大长度1023，留一个字节给终止符
    strncpy(args, p, args_len - 1);
    args[args_len - 1] = '\0';

    return 0;
}

/**
 * 解析命令行参数：支持 -ip IPaddress 与 -port n；未指定使用默认
 * 成功返回0，失败返回非0
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
                return -1;
            }
            snprintf(ip_out, ip_len, "%s", argv[i + 1]);
            i++;
        }
        else if (strcmp(argv[i], "-port") == 0)
        {
            if (i + 1 >= argc)
            {
                return -1;
            }
            char *endp = NULL;
            long p = strtol(argv[i + 1], &endp, 10);
            if (endp && *endp != '\0')
            {
                return -1;
            }
            if (p <= 0 || p > 65535)
            {
                return -1;
            }
            *port_out = (int)p;
            i++;
        }
        else
        {
            return -1;
        }
    }

    return 0;
}