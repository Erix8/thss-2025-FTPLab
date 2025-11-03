#include "ui_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * 读取用户输入的命令字符串
 * @return 动态分配的输入字符串（需调用者用free释放），用户退出时返回NULL
 */
char *ui_read_input()
{
    char buf[1024];
    if (fgets(buf, sizeof(buf), stdin) == NULL)
    {
        return NULL; // 用户输入Ctrl+D
    }
    // 移除末尾的换行符
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n')
    {
        buf[len - 1] = '\0';
    }
    // 动态分配内存返回
    char *input = strdup(buf);
    return input;
}

/**
 * 打印服务器响应信息或操作结果提示
 * @param msg 要打印的信息字符串
 */
void ui_print_msg(const char *msg)
{
    if (msg)
        printf("%s\n", msg);
}