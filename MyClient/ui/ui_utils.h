#ifndef UI_UTILS_H
#define UI_UTILS_H

#include <stddef.h>

// 读取用户输入（返回输入字符串，需调用者释放内存）
char *ui_read_input();

// 打印服务器响应或操作结果
void ui_print_msg(const char *msg);

// 解析命令行参数：支持 -ip IPaddress 与 -port n；未指定使用默认值
// 返回0表示成功，非0表示失败（会输出用法说明）
int ui_parse_args(int argc, char *argv[], char *ip_out, size_t ip_len, int *port_out);

#endif