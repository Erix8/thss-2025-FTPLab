#include "client_cmds.h"
#include "../net/client_socket.h"
#include "../ui/ui_utils.h"
#include "../utils/utils.h"
#include <string.h>
/**
 * 处理用户输入的客户端命令（转换为FTP协议命令）
 * @param ctrl_fd 控制连接文件描述符
 * @param state 客户端当前认证状态指针（可能被更新）
 * @param input 用户输入的命令字符串
 * @return 0表示处理成功，非0表示处理失败
 */
int client_handle_input(int ctrl_fd, ClientState *state, const char *input)
{
    if (!input || !state)
        return -1;

    char cmd[16], args[1024];
    if (utils_split_cmd(input, cmd, sizeof(cmd), args, sizeof(args)) != 0)
    {
        return 0;
    }

    // 发送用户输入的命令到服务器（直接透传）
    client_send_cmd(ctrl_fd, input);

    // 接收服务器响应
    char resp[1024];
    int code = client_recv_resp(ctrl_fd, resp, sizeof(resp));
    if (code == -1)
    {
        // ui_print_msg("Failed to receive response");
        return -1;
    }

    ui_print_msg(resp);

    if (strcmp(cmd, "QUIT") == 0 && code == 221)
    {
        return 1; // 退出标志
    }
    return 0;
}