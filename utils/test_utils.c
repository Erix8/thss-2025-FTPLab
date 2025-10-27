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
    input = "USER test";
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
}

int main()
{
    return 0;
}