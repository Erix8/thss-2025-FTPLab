#include "net/client_socket.h"
#include "ui/ui_utils.h"
#include "cmd/client_cmds.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
    // 默认参数
    char ip[64] = "127.0.0.1";
    int port = 21;

    // 解析命令行参数：-ip IPaddress -port n
    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "-ip") == 0)
        {
            if (i + 1 >= argc)
            {
                ui_print_msg("Usage: ./client [-ip IPaddress] [-port n]");
                return 1;
            }
            snprintf(ip, sizeof(ip), "%s", argv[i + 1]);
            i++;
        }
        else if (strcmp(argv[i], "-port") == 0)
        {
            if (i + 1 >= argc)
            {
                ui_print_msg("Usage: ./client [-ip IPaddress] [-port n]");
                return 1;
            }
            port = atoi(argv[i + 1]);
            i++;
        }
        else
        {
            ui_print_msg("Usage: ./client [-ip IPaddress] [-port n]");
            return 1;
        }
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
        client_handle_input(ctrl_fd, &state, input);
        free(input);
    }
    // 断开连接
    client_disconnect(ctrl_fd);
    return 0;
}