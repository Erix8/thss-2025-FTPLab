#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>

/**
 * 拼接根目录与相对路径，生成安全的绝对路径（防止路径越权）
 * @param root 根目录路径（如"/ftp_root"）
 * @param relative 相对路径（如"subdir/file.txt"）
 * @param result 输出拼接后的绝对路径缓冲区（需足够大）
 * @param result_len 输出缓冲区的大小（包含终止符 \0，建议用 PATH_MAX）
 * @return 指向result的指针，失败返回NULL
 */
char *utils_join_path(const char *root, const char *relative, char *result, size_t result_len);

/**
 * 检查目标路径是否在根目录范围内（防止通过../越权访问）
 * @param root 根目录路径（如"/ftp_root"）
 * @param target 待检查的目标路径（绝对路径）
 * @return 1表示合法（在根目录内），0表示非法（越权访问）
 */
int utils_check_path(const char *root, const char *target);

/**
 * 分割命令行字符串为命令和参数
 * @param cmd_line 完整命令行（如"RETR file.txt"）
 * @param cmd 输出命令缓冲区（至少16字节）
 * @param cmd_len 输出命令缓冲区长度
 * @param args 输出参数缓冲区（至少1024字节）
 * @param args_len 输出参数缓冲区长度
 * @return 0：成功；-1：参数无效或缓冲区不足；-2：空命令
 */
int utils_split_cmd(const char *cmd_line, char *cmd, size_t cmd_len, char *args, size_t args_len);

#endif