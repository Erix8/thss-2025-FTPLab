#ifndef CLIENT_CMDS_H
#define CLIENT_CMDS_H

// 客户端状态（是否已认证）
typedef enum
{
    CLIENT_STATE_UNAUTH,
    CLIENT_STATE_AUTHED
} ClientState;

// 处理用户输入的命令
int client_handle_input(int ctrl_fd, ClientState *state, const char *input);

#endif