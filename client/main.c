#include "net/client_socket.h"
#include "ui/ui_utils.h"
#include "cmd/client_cmds.h"

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        ui_print_msg("Usage: ./client <server_ip> <port>");
        return 1;
    }
    // 连接服务器
    int ctrl_fd = client_connect(argv[1], atoi(argv[2]));
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
        ui_show_prompt();
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