#include "net/client_socket.h"
#include "ui/ui_utils.h"
#include "cmd/client_cmds.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
    char ip[64];
    int port = 0;
    if (ui_parse_args(argc, argv, ip, sizeof(ip), &port) != 0)
    {
        return 1;
    }
    // 连接服务器
    int ctrl_fd = client_connect(ip, port);
    if (ctrl_fd == -1)
    {
        return 1;
    }
    // 处理服务器欢迎信息
    char resp[1024];
    client_recv_resp(ctrl_fd, resp, sizeof(resp));
    ui_print_msg(resp);

    ClientState state = CLIENT_STATE_UNAUTH;
    // 交互循环
    while (1)
    {
        char *input = ui_read_input();
        if (input == NULL)
            break; // 用户输入exit或Ctrl+C
        // 处理命令
        int should_exit = client_handle_input(ctrl_fd, &state, input);
        free(input);
        if (should_exit == 1)
            break;
        else if (should_exit == -1)
        {
            // ui_print_msg("Error processing command.");
            return 1;
        }
    }
    // 断开连接
    client_disconnect(ctrl_fd);
    return 0;
}