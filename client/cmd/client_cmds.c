#include "client_cmds.h"

/**
 * 处理用户输入的客户端命令（转换为FTP协议命令）
 * @param ctrl_fd 控制连接文件描述符
 * @param state 客户端当前认证状态指针（可能被更新）
 * @param input 用户输入的命令字符串（如"get file.txt"）
 * @return 0表示处理成功，非0表示处理失败
 */
int client_handle_input(int ctrl_fd, ClientState *state, const char *input);