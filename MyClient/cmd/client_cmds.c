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
 * @return 0表示处理成功，1表示退出，-1表示错误
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

    if (strcmp(cmd, "PASV") == 0)
    {
        return 0;
    }
    else if (strcmp(cmd, "PORT") == 0)
    {
        return 0;
    }
    else if (strcmp(cmd, "RETR") == 0)
    {
        return 0;
    }
    else if (strcmp(cmd, "STOR") == 0)
    {
        return 0;
    }
    else if (strcmp(cmd, "LIST") == 0)
    {
        return 0;
    }
    else
    {
        // 其他一般指令直接传送给服务器
        client_send_cmd(ctrl_fd, input);
        char resp[1024];
        client_recv_resp(ctrl_fd, resp, sizeof(resp));
        ui_print_msg(resp);
        if (strcmp(cmd, "QUIT") == 0)
            return 1; // 退出标志
        return 0;
    }
}