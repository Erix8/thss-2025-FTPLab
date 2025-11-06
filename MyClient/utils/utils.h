#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>

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

/**
 * 解析FTP PORT命令参数，提取IP地址和端口号
 * @param args PORT命令参数字符串（格式如"h1,h2,h3,h4,p1,p2"）
 * @param ip_buf 输出IP地址缓冲区（建议至少16字节）
 * @param ip_len 输出IP地址缓冲区长度
 * @param port_out 输出端口号指针
 * @return 0：成功；-1：参数无效或格式错误
 */
int parse_port_arg(const char *args, char *ip_buf, size_t ip_len, uint16_t *port_out);

/**
 * 解析命令行参数：支持 -ip IPaddress 与 -port n；未指定使用默认值
 * @param argc 参数个数
 * @param argv 参数字符串数组
 * @param ip_out 输出IP地址缓冲区
 * @param ip_len 输出IP地址缓冲区长度
 * @param port_out 输出端口号指针
 * @return 0：成功；非0：失败
 */
int ui_parse_args(int argc, char *argv[], char *ip_out, size_t ip_len, int *port_out);

#endif