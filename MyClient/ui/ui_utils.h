#ifndef UI_UTILS_H
#define UI_UTILS_H

#include <stddef.h>

// 读取用户输入（返回输入字符串，需调用者释放内存）
char *ui_read_input();

// 打印服务器响应或操作结果
void ui_print_msg(const char *msg);

#endif