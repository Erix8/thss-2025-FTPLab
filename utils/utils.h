#ifndef UTILS_H
#define UTILS_H

// 拼接路径（防止路径越权，如将root_dir + relative_path转为绝对路径）
char *utils_join_path(const char *root, const char *relative, char *result);

// 检查路径是否在根目录内（防止../越权访问，返回1合法，0非法）
int utils_check_path(const char *root, const char *target);

// 字符串分割（将"PORT 192,168,1,1,128,80"分割为命令和参数）
void utils_split_cmd(const char *cmd_line, char *cmd, char *args);

#endif