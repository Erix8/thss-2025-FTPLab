#ifndef FTP_CMDS_H
#define FTP_CMDS_H

#include "../conn/client_conn.h"

typedef enum
{
    XFER_RETR,
    XFER_STOR
} XferType;

typedef struct
{
    ClientConn *conn;
    XferType type;
    char filename[PATH_MAX];
} XferTask;

/**
 * 处理客户端发送的命令行
 * @param conn 客户端连接信息结构体指针
 * @param cmd 客户端发送的命令字符串
 * @param args 客户端发送的命令参数字符串
 */
void cmd_process(ClientConn *conn, const char *cmd, const char *args);

#endif