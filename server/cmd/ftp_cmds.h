#ifndef FTP_CMDS_H
#define FTP_CMDS_H

#include "../conn/client_conn.h"

/**
 * 处理客户端发送的命令行
 * @param conn 客户端连接信息结构体指针
 * @param cmd 客户端发送的命令字符串
 * @param args 客户端发送的命令参数字符串
 * @return FTP响应码
 */
int cmd_process(ClientConn *conn, const char *cmd, const char *args)
{
}

// 以下为内部命令处理函数（仅在.c中实现，.h不暴露）
// int cmd_handle_user(ClientConn* conn, const char* args);  // 处理USER命令
// int cmd_handle_pass(ClientConn* conn, const char* args);  // 处理PASS命令
// int cmd_handle_port(ClientConn* conn, const char* args);  // 处理PORT命令
// int cmd_handle_pasv(ClientConn* conn, const char* args);  // 处理PASV命令
// int cmd_handle_retr(ClientConn* conn, const char* args);  // 处理RETR命令（下载）
// int cmd_handle_stor(ClientConn* conn, const char* args);  // 处理STOR命令（上传）
// int cmd_handle_cwd(ClientConn* conn, const char* args);   // 处理CWD命令（切换目录）
// int cmd_handle_list(ClientConn* conn, const char* args);  // 处理LIST命令（列目录）

#endif