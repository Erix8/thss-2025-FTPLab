#include "ui_utils.h"

/**
 * 显示FTP客户端提示符（如"ftp> "）
 */
void ui_show_prompt();

/**
 * 读取用户输入的命令字符串
 * @return 动态分配的输入字符串（需调用者用free释放），用户退出时返回NULL
 */
char *ui_read_input();

/**
 * 打印服务器响应信息或操作结果提示
 * @param msg 要打印的信息字符串
 */
void ui_print_msg(const char *msg);