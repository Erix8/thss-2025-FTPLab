#include "utils.h"
#include "utils.c"
#include <limits.h> // 用于 PATH_MAX
#include <stdio.h>
#include <string.h>

void test_utils_split_cmd()
{
    char cmd[16];
    char args[1024];
    const char *input;
    // test case 1
    memset(cmd, 0, sizeof(cmd));
    memset(args, 0, sizeof(args));
    input = "USEr test";
    printf("Return value: %d; Command: '%s'; Args: '%s'\n",
           utils_split_cmd(input, cmd, sizeof(cmd), args, sizeof(args)), cmd, args);
    // test case 2
    memset(cmd, 0, sizeof(cmd));
    memset(args, 0, sizeof(args));
    input = "   PASS  mypassword  ";
    printf("Return value: %d; Command: '%s'; Args: '%s'\n",
           utils_split_cmd(input, cmd, sizeof(cmd), args, sizeof(args)), cmd, args);
    // test case 3
    memset(cmd, 0, sizeof(cmd));
    memset(args, 0, sizeof(args));
    input = "QUIT";
    printf("Return value: %d; Command: '%s'; Args: '%s'\n",
           utils_split_cmd(input, cmd, sizeof(cmd), args, sizeof(args)), cmd, args);
    // test case 4
    memset(cmd, 0, sizeof(cmd));
    memset(args, 0, sizeof(args));
    input = "HELP";
    printf("Return value: %d; Command: '%s'; Args: '%s'\n",
           utils_split_cmd(input, cmd, 0, args, sizeof(args)), cmd, args);
    // test case 5
    memset(cmd, 0, sizeof(cmd));
    memset(args, 0, sizeof(args));
    input = "   ";
    printf("Return value: %d; Command: '%s'; Args: '%s'\n",
           utils_split_cmd(input, cmd, sizeof(cmd), args, sizeof(args)), cmd, args);
    // test case 6
    memset(cmd, 0, sizeof(cmd));
    memset(args, 0, sizeof(args));
    input = "ABCDEFGHIJKLMNOP arg1234567890";
    printf("Return value: %d; Command: '%s'; Args: '%s'\n",
           utils_split_cmd(input, cmd, sizeof(cmd), args, sizeof(args)), cmd, args);
}

void test_utils_check_path()
{
    // 测试用例1: 目标路径等于根目录
    printf("Test 1: %d (预期1)\n", utils_check_path("/ftp_root", "/ftp_root"));

    // 测试用例2: 目标路径为根目录子文件
    printf("Test 2: %d (预期1)\n", utils_check_path("/ftp_root", "/ftp_root/file.txt"));

    // 测试用例3: 目标路径为根目录子目录
    printf("Test 3: %d (预期1)\n", utils_check_path("/ftp_root", "/ftp_root/subdir/"));

    // 测试用例4: 根目录为系统根目录，目标为子路径
    printf("Test 4: %d (预期1)\n", utils_check_path("/", "/home/user/file"));

    // 测试用例5: 目标路径长度小于根目录
    printf("Test 5: %d (预期0)\n", utils_check_path("/ftp_root", "/ftp"));

    // 测试用例6: 目标路径前缀匹配但非子路径
    printf("Test 6: %d (预期0)\n", utils_check_path("/ftp_root", "/ftp_root_extra"));

    // 测试用例8: 空指针参数
    printf("Test 8: %d (预期0)\n", utils_check_path(NULL, "/ftp_root/file.txt"));
    printf("Test 9: %d (预期0)\n", utils_check_path("/ftp_root", NULL));

    // 测试用例10: 根目录为单斜杠，目标路径正确
    printf("Test 10: %d (预期1)\n", utils_check_path("/", "/"));

    // 测试用例11: 目标路径刚好在根目录外
    printf("Test 11: %d (预期0)\n", utils_check_path("/a", "/"));
}

void test_utils_join_path()
{
    char result[PATH_MAX];
    const char *root;
    const char *relative;

    // 测试用例1: 正常拼接（无特殊路径符）
    root = "/ftp_root";
    relative = "subdir/file.txt";
    printf("Test 1: ");
    if (utils_join_path(root, relative, result, sizeof(result)))
    {
        printf("Result: '%s' (预期 '/ftp_root/subdir/file.txt')\n", result);
    }
    else
    {
        printf("返回NULL（预期有效路径）\n");
    }

    // 测试用例2: 相对路径包含当前目录.
    root = "/ftp_root";
    relative = "subdir/./file.txt";
    printf("Test 2: ");
    if (utils_join_path(root, relative, result, sizeof(result)))
    {
        printf("Result: '%s' (预期 '/ftp_root/subdir/file.txt')\n", result);
    }
    else
    {
        printf("返回NULL（预期有效路径）\n");
    }

    // 测试用例3: 相对路径包含上级目录..（合法范围内）
    root = "/ftp_root";
    relative = "a/b/../c/file.txt";
    printf("Test 3: ");
    if (utils_join_path(root, relative, result, sizeof(result)))
    {
        printf("Result: '%s' (预期 '/ftp_root/a/c/file.txt')\n", result);
    }
    else
    {
        printf("返回NULL（预期有效路径）\n");
    }

    // 测试用例4: 相对路径越权（通过..超出根目录）
    root = "/ftp_root";
    relative = "../etc/passwd";
    printf("Test 4: ");
    if (utils_join_path(root, relative, result, sizeof(result)))
    {
        printf("Result: '%s'（预期NULL）\n", result);
    }
    else
    {
        printf("返回NULL（符合预期）\n");
    }

    // 测试用例5: 根目录为系统根目录/
    root = "/";
    relative = "home/user/data";
    printf("Test 5: ");
    if (utils_join_path(root, relative, result, sizeof(result)))
    {
        printf("Result: '%s' (预期 '/home/user/data')\n", result);
    }
    else
    {
        printf("返回NULL（预期有效路径）\n");
    }

    // 测试用例6: 根目录后无斜杠，相对路径前无斜杠
    root = "/ftp_root";
    relative = "file.txt";
    printf("Test 6: ");
    if (utils_join_path(root, relative, result, sizeof(result)))
    {
        printf("Result: '%s' (预期 '/ftp_root/file.txt')\n", result);
    }
    else
    {
        printf("返回NULL（预期有效路径）\n");
    }

    // 测试用例7: 相对路径为空字符串
    root = "/ftp_root";
    relative = "";
    printf("Test 7: ");
    if (utils_join_path(root, relative, result, sizeof(result)))
    {
        printf("Result: '%s' (预期 '/ftp_root')\n", result);
    }
    else
    {
        printf("返回NULL（预期有效路径）\n");
    }

    // 测试用例8: 缓冲区大小不足
    root = "/ftp_root";
    relative = "long/path/that/should/exceed/buffer";
    char small_buf[10];
    printf("Test 8: ");
    if (utils_join_path(root, relative, small_buf, sizeof(small_buf)))
    {
        printf("Result: '%s'（预期NULL）\n", small_buf);
    }
    else
    {
        printf("返回NULL（符合预期）\n");
    }

    // 测试用例9: 空指针参数
    printf("Test 9: ");
    if (utils_join_path(NULL, relative, result, sizeof(result)))
    {
        printf("返回非NULL（预期NULL）\n");
    }
    else
    {
        printf("返回NULL（符合预期）\n");
    }

    printf("Test 10: ");
    if (utils_join_path(root, NULL, result, sizeof(result)))
    {
        printf("返回非NULL（预期NULL）\n");
    }
    else
    {
        printf("返回NULL（符合预期）\n");
    }

    printf("Test 11: ");
    if (utils_join_path(root, relative, NULL, sizeof(result)))
    {
        printf("返回非NULL（预期NULL）\n");
    }
    else
    {
        printf("返回NULL（符合预期）\n");
    }

    // 测试用例12: 处理连续斜杠
    root = "/ftp_root//";
    relative = "//subdir//file.txt";
    printf("Test 12: ");
    if (utils_join_path(root, relative, result, sizeof(result)))
    {
        printf("Result: '%s' (预期 '/ftp_root/subdir/file.txt')\n", result);
    }
    else
    {
        printf("返回NULL（预期有效路径）\n");
    }
}

// 在main函数中添加调用
int main()
{
    test_utils_split_cmd();
    test_utils_check_path();
    test_utils_join_path();
    return 0;
}