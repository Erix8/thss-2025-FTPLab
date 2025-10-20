#include "utils.h"
#include <limits.h> // 用于 PATH_MAX

int main()
{
    // 声明缓冲区
    char result[PATH_MAX];
    char cmd[16];
    char args[1024];

    // 调用三个函数（参数仅为编译检查，无实际意义）
    utils_join_path("/root", "file", result, PATH_MAX);
    utils_check_path("/root", "/root/file");
    utils_split_cmd("GET file.txt", cmd, 16, args, 1024);

    return 0;
}