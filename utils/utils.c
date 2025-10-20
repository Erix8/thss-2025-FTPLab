#include "utils.h"
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <ctype.h>
#define CMD_MAX_LEN 16
#define ARGS_MAX_LEN 1024

/**
 * 拼接根目录与相对路径，生成安全的绝对路径（防止路径越权）
 * @param root 根目录路径（如"/ftp_root"）
 * @param relative 相对路径（如"subdir/file.txt"）
 * @param result 输出拼接后的绝对路径缓冲区（需足够大）
 * @param result_len 输出缓冲区的大小（包含终止符 \0，建议用 PATH_MAX）
 * @return 指向result的指针，失败返回NULL
 */
char *utils_join_path(const char *root, const char *relative, char *result, size_t result_len)
{
    // 检查参数是否合格，路径不能为空
    if (!root || !relative || !result)
        return NULL;

    // 临时缓冲区存储拼接后的原始路径
    char temp[PATH_MAX];
    if (snprintf(temp, sizeof(temp), "%s/%s", root, relative) >= sizeof(temp))
        return NULL; // 路径过长

    // 规范化路径（处理.和..）
    char normalized[PATH_MAX];
    char *p = temp;
    char *q = normalized;
    char *start = normalized;

    while (*p)
    {
        // 跳过连续斜杠
        while (*p == '/')
            p++;

        // 到达temp结尾
        if (!*p)
            break;

        // 处理当前目录.
        // TODO:如果p+1超出缓冲区代码会不会不安全？
        if (*p == '.' && (*(p + 1) == '\0' || *(p + 1) == '/'))
        {
            p += (*(p + 1) == '/') ? 2 : 1;
            continue;
        }

        // 处理上级目录..
        if (*p == '.' && *(p + 1) != '\0' && *(p + 1) == '.' && (*(p + 2) == '\0' || *(p + 2) == '/'))
        {
            // 不能回退到根目录之外
            if (q > start)
            {
                // 回退到上一个目录
                q--;
                while (q > start && *q != '/')
                    q--;
            }
            p += (*(p + 2) == '/') ? 3 : 2;
            continue;
        }

        // 复制当前目录名
        if (q != start)
            *q++ = '/';
        while (*p && *p != '/')
        {
            *q++ = *p++;
        }
    }
    *q = '\0';

    // 移除末尾多余斜杠（根目录"/"除外），首部斜杠需要保留
    if (q > start + 1 && *(q - 1) == '/')
        q--; // 仅当路径长度 > 1 时才移除末尾斜杠
    *q = '\0';

    // 检查规范化后的路径是否在根目录内
    if (!utils_check_path(root, normalized))
    {
        return NULL;
    }

    if (strlen(normalized) >= result_len)
        return NULL;
    strncpy(result, normalized, result_len - 1);
    result[result_len - 1] = '\0';
    return result;
}

/**
 * 检查目标路径是否在根目录范围内（防止通过../越权访问）
 * @param root 根目录路径（如"/ftp_root"）
 * @param target 待检查的目标路径（绝对路径）
 * @return 1表示合法（在根目录内），0表示非法（越权访问）
 */
int utils_check_path(const char *root, const char *target)
{
    // 检验参数是否合格
    if (!root || !target)
        return 0;

    size_t root_len = strlen(root);
    size_t target_len = strlen(target);

    // 目标路径长度必须大于等于根目录长度
    if (target_len < root_len)
        return 0;

    // 检查前缀是否匹配
    if (strncmp(root, target, root_len) != 0)
        return 0;

    // 根目录是整个路径或后续为路径分隔符
    if (target_len == root_len)
        return 1;
    // 根目录后紧跟路径分隔符或根目录本身就是根路径"/"
    if (target[root_len] == '/' || (root_len == 1 && root[0] == '/'))
        return 1;

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
    size_t cmd_len = p - cmd_start;
    // 命令转为大写并复制（限制最大长度15，留一个字节给终止符）
    if (cmd_len > CMD_MAX_LEN)
        cmd_len = CMD_MAX_LEN;
    for (size_t i = 0; i < cmd_len; i++)
    {
        char c = cmd_start[i];
        cmd[i] = (c >= 'a' && c <= 'z') ? (c - 32) : c;
    }
    cmd[cmd_len] = '\0';

    // 提取参数（剩余部分，跳过中间空格）
    while (*p == ' ')
        p++;
    // 限制参数最大长度1023，留一个字节给终止符
    strncpy(args, p, args_len - 1);
    args[args_len] = '\0';

    return 0;
}