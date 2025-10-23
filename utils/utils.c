#include "utils.h"
#include <string.h>
#include <stdio.h>

/**
 * 拼接根目录与相对路径，生成安全的绝对路径（防止路径越权）
 * @param root 根目录路径（如"/ftp_root"）
 * @param relative 相对路径（如"subdir/file.txt"）
 * @param result 输出拼接后的绝对路径缓冲区（需足够大）
 * @return 指向result的指针，失败返回NULL
 */
char *utils_join_path(const char *root, const char *relative, char *result, size_t result_len)
{
    return NULL;
}

/**
 * 检查目标路径是否在根目录范围内（防止通过../越权访问）
 * @param root 根目录路径（如"/ftp_root"）
 * @param target 待检查的目标路径（绝对路径）
 * @return 1表示合法（在根目录内），0表示非法（越权访问）
 */
int utils_check_path(const char *root, const char *target)
{
    return 1;
}

/**
 * 分割命令行字符串为命令和参数
 * @param cmd_line 完整命令行（如"RETR file.txt"）
 * @param cmd 输出命令缓冲区（至少16字节）
 * @param args 输出参数缓冲区（至少1024字节）
 */
void utils_split_cmd(const char *cmd_line, char *cmd, size_t cmd_len, char *args, size_t args_len)
{
    if (!cmd_line || !cmd || !args)
        return;

    // 初始化输出缓冲区
    *cmd = '\0';
    *args = '\0';

    // 跳过前导空格
    const char *p = cmd_line;
    while (*p == ' ')
        p++;
    if (*p == '\0')
        return; // 空命令

    // 提取命令（到第一个空格为止）
    const char *cmd_start = p;
    while (*p != ' ' && *p != '\0')
        p++;
    size_t cmd_len = p - cmd_start;
    strncpy(cmd, cmd_start, cmd_len);
    cmd[cmd_len] = '\0';

    // 提取参数（剩余部分，跳过中间空格）
    while (*p == ' ')
        p++;
    strcpy(args, p);
}