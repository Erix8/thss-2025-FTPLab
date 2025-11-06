#ifndef CLIENT_CMDS_H
#define CLIENT_CMDS_H

// 数据连接模式
typedef enum
{
    DATA_MODE_NONE, // 尚未选择数据连接模式
    DATA_MODE_PORT, // PORT模式（主动）
    DATA_MODE_PASV  // PASV模式（被动）
} DataMode;

typedef struct
{
    int ctrl_fd;        // 控制连接文件描述符
    int data_fd;        // 数据连接文件描述符
    DataMode data_mode; // 数据连接模式
} Client;

void client_init(Client *client);

// 处理用户输入的命令
int client_handle_input(Client *client, const char *input);

#endif