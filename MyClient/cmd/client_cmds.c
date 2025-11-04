#include "client_cmds.h"
#include "../net/client_socket.h"
#include "../ui/ui_utils.h"
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

    // 处理退出命令
    if (strcmp(input, "exit") == 0 || strcmp(input, "quit") == 0)
    {
        client_send_cmd(ctrl_fd, "QUIT");
        char resp[1024];
        client_recv_resp(ctrl_fd, resp, sizeof(resp));
        ui_print_msg(resp);
        return 0;
    }

    // 发送用户输入的命令到服务器（直接透传）
    client_send_cmd(ctrl_fd, input);

    // 接收服务器响应
    char resp[1024];
    int code = client_recv_resp(ctrl_fd, resp, sizeof(resp));
    if (code == -1)
    {
        ui_print_msg("Failed to receive response");
        return -1;
    }

    ui_print_msg(resp);

    // 更新认证状态
    if (code == 230)
    { // 230表示登录成功
        *state = CLIENT_STATE_AUTHED;
    }
    return 0;
}