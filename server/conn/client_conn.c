#include "client_conn.h"
#include "../net/socket_utils.h"
#include "../../utils/utils.h"
#include "../cmd/ftp_cmds.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>

// 声明全局客户端连接列表指针（用于信号处理函数访问）
static ClientConnList *g_client_list = NULL;

// 信号处理函数：收到SIGINT时清理资源并退出
static void handle_sigint(int signum)
{
    if (signum == SIGINT)
    {
        printf("\nReceived Ctrl+C, shutting down server...\n");
        if (g_client_list)
        {
            client_conn_list_destroy(g_client_list);
        }
        exit(0); // 退出程序
    }
}

/**
 * 初始化动态客户端连接列表
 * @param init_capacity 初始容量（推荐设为ServerConfig.max_conn的1.5倍，避免频繁扩容）
 * @return 动态列表指针，失败返回NULL
 */
ClientConnList *client_conn_list_init(size_t init_capacity)
{
    if (init_capacity < 4)
        init_capacity = 4;
    // 分配列表结构体内存
    ClientConnList *list = malloc(sizeof(ClientConnList));
    if (!list)
    {
        perror("malloc ClientConnList failed");
        return NULL;
    }
    // 分配初始容量的客户端连接数组
    list->data = malloc(sizeof(ClientConn) * init_capacity);
    if (!list->data)
    {
        perror("malloc ClientConn array failed");
        free(list); // 回滚：释放已分配的结构体
        return NULL;
    }
    // 初始化列表状态：所有连接槽位设为空闲（ctrl_fd=-1）
    list->capacity = init_capacity;
    list->used = 0;
    for (size_t i = 0; i < init_capacity; i++)
    {
        // 显式初始化整个结构体，避免未初始化字段带来问题
        memset(&list->data[i], 0, sizeof(ClientConn));
        list->data[i].ctrl_fd = -1; // -1表示槽位空闲
        list->data[i].data_fd = -1;
    }
    return list;
}

static int client_conn_list_resize(ClientConnList *list, size_t new_capacity)
{
    if (!list)
        return -1;
    // 新容量不能小于已使用数量（避免数据丢失）
    if (new_capacity < list->used)
    {
        fprintf(stderr, "new capacity (%zu) < used count (%zu)\n", new_capacity, list->used);
        return -1;
    }
    // 重新分配内存（保留原有数据）
    ClientConn *old_data = list->data;
    size_t old_capacity = list->capacity;
    ClientConn *new_data = realloc(list->data, sizeof(ClientConn) * new_capacity);
    if (!new_data)
    {
        perror("realloc ClientConn array failed");
        return -1;
    }
    // 初始化新增的槽位（空闲状态）
    if (new_capacity > old_capacity)
    {
        for (size_t i = old_capacity; i < new_capacity; i++)
        {
            memset(&new_data[i], 0, sizeof(ClientConn));
            new_data[i].ctrl_fd = -1;
            new_data[i].data_fd = -1;
        }
    }
    // 更新列表信息
    list->data = new_data;
    list->capacity = new_capacity;
    printf("ClientConnList resized: %zu -> %zu\n", old_capacity, new_capacity);
    return 0;
}

/**
 * 向动态列表添加新客户端连接
 * @param list 动态列表指针
 * @param new_conn 待添加的客户端连接（已初始化）
 * @return 0成功，-1失败（容量不足且扩容失败）
 */
int client_conn_list_add(ClientConnList *list, ClientConn *new_conn)
{
    if (!list || !new_conn || new_conn->ctrl_fd == -1)
    {
        fprintf(stderr, "invalid param for client_conn_list_add\n");
        return -1;
    }

    // 步骤1：检查是否有空闲槽位（优先复用空闲槽位，避免扩容）
    size_t free_idx = list->capacity; // 初始设为无效索引
    for (size_t i = 0; i < list->capacity; i++)
    {
        if (list->data[i].ctrl_fd == -1)
        {
            free_idx = i;
            break;
        }
    }

    // 步骤2：无空闲槽位，触发扩容（新容量=原容量*2）
    if (free_idx == list->capacity)
    {
        size_t new_capacity = (list->capacity == 0) ? 4 : list->capacity * 2; // 初始容量为4
        if (client_conn_list_resize(list, new_capacity) != 0)
        {
            fprintf(stderr, "resize client connection list failed.\n");
            return -1;
        }
        // 扩容后再重新扫描第一个空闲槽位，避免假设 list->used==first_new_slot
        free_idx = list->capacity; // set to invalid
        for (size_t i = 0; i < list->capacity; i++)
        {
            if (list->data[i].ctrl_fd == -1)
            {
                free_idx = i;
                break;
            }
        }
        if (free_idx == list->capacity)
        {
            // 理论上不应该到这里
            fprintf(stderr, "no free slot found after resize\n");
            return -1;
        }
    }

    // 步骤3：添加新连接到空闲槽位
    // 复制并确保所有字段被写入（避免残留未初始化数据）
    memset(&list->data[free_idx], 0, sizeof(ClientConn));
    list->data[free_idx] = *new_conn; // 复制连接信息
    list->used++;
    return 0;
}

/**
 * 从动态列表移除指定ctrl_fd的客户端连接（关闭socket+标记槽位空闲）
 * @param list 动态列表指针
 * @param ctrl_fd 待移除的客户端控制连接fd
 */
void client_conn_list_remove(ClientConnList *list, int ctrl_fd)
{
    if (!list || ctrl_fd < 0)
        return;

    // 查找并移除目标连接
    for (size_t i = 0; i < list->capacity; i++)
    {
        if (list->data[i].ctrl_fd == ctrl_fd)
        {
            // 仅关闭看起来像 socket 的 fd（避免意外关闭 stdin/stdout/stderr）
            if (list->data[i].ctrl_fd >= 3)
                socket_close(list->data[i].ctrl_fd);
            if (list->data[i].data_fd >= 3)
                socket_close(list->data[i].data_fd);
            if (list->data[i].pasv_listen_fd >= 3)
                socket_close(list->data[i].pasv_listen_fd);
            // 标记槽位为空闲
            memset(&list->data[i], 0, sizeof(ClientConn));
            list->data[i].ctrl_fd = -1;
            list->data[i].data_fd = -1;
            list->data[i].pasv_listen_fd = -1;
            list->used--;
            printf("Client %d removed (used: %zu/%zu)\n", ctrl_fd, list->used, list->capacity);
            // 当空闲槽位过多时缩容（避免内存浪费）
            // 条件：used < capacity/2 且 capacity > 4（最小容量保留4）
            if (list->used < list->capacity / 2 && list->capacity / 2 >= 4)
            {
                size_t new_capacity = list->capacity / 2;
                client_conn_list_resize(list, new_capacity);
            }
            break;
        }
    }
}

/**
 * 销毁动态列表，关闭所有客户端连接并释放内存
 * @param list 动态列表指针（销毁后会置为NULL）
 */
void client_conn_list_destroy(ClientConnList *list)
{
    if (!list)
        return;

    // 关闭所有活跃连接
    for (size_t i = 0; i < list->capacity; i++)
    {
        if (list->data[i].ctrl_fd != -1)
        {
            socket_close(list->data[i].ctrl_fd);
            if (list->data[i].data_fd != -1)
            {
                socket_close(list->data[i].data_fd);
            }
            if (list->data[i].pasv_listen_fd != -1)
            {
                socket_close(list->data[i].pasv_listen_fd);
            }
        }
    }
    // 释放数组和列表结构体
    free(list->data);
    free(list);
    list = NULL; // 避免野指针
    printf("ClientConnList destroyed\n");
}

/**
 * 初始化客户端连接信息结构体
 * @param conn 客户端连接结构体指针
 * @param ctrl_fd 控制连接文件描述符
 * @param root_dir FTP服务器根目录路径
 */
void client_conn_init(ClientConn *conn, int ctrl_fd, const char *root_dir)
{
    memset(conn, 0, sizeof(ClientConn));
    conn->ctrl_fd = ctrl_fd;
    conn->auth_state = AUTH_STATE_UNAUTH;
    conn->pending_user_anon = 0;
    conn->data_fd = -1;
    conn->data_mode = DATA_MODE_NONE;
    conn->data_host[0] = '\0';
    conn->data_port = 0;
    conn->pasv_listen_fd = -1;
    conn->xfer_in_progress = 0;
    strcpy(conn->current_dir, root_dir); // 初始目录为根目录
}

// 处理单个客户端请求
int handle_client_cmd(ClientConn *conn)
{
    char buf[1024];
    ssize_t n = socket_recv(conn->ctrl_fd, buf, sizeof(buf));
    if (n <= 0)
    {
        // 客户端断开连接（由conn_manager_run处理移除）
        return 1;
    }

    // 传输期间：忽略该客户端的控制命令
    if (conn->xfer_in_progress)
    {
        return 0;
    }

    // 解析命令，若不合法返回500
    char cmd[16], args[1024];
    if (utils_split_cmd(buf, cmd, sizeof(cmd), args, sizeof(args)) != 0)
    {
        socket_send(conn->ctrl_fd, "500 Invalid command format.\r\n");
        return 0;
    }

    // 若为退出命令返回1，其余命令返回0
    if (strcmp(cmd, "QUIT") == 0)
    {
        socket_send(conn->ctrl_fd, "221 Goodbye.\r\n");
        return 1;
    }
    else
    {
        // 处理其他命令，返回对应信息
        cmd_process(conn, cmd, args);
        return 0;
    }
}

/**
 * 运行连接管理器，处理多客户端连接（基于select循环）
 * @param listen_fd 监听文件描述符
 * @param config 服务器配置结构体指针
 */
void conn_manager_run(int listen_fd, const ServerConfig *config)
{
    // 初始化动态列表
    ClientConnList *client_list = client_conn_list_init(4);
    if (!client_list)
    {
        fprintf(stderr, "init ClientConnList failed\n");
        return;
    }

    g_client_list = client_list;

    // 注册SIGINT信号处理函数
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigaction(SIGINT, &sa, NULL);

    fd_set read_fds;
    int max_fd = listen_fd; // select需要的最大fd

    while (1)
    {
        // 步骤1：初始化select读集合
        FD_ZERO(&read_fds);
        FD_SET(listen_fd, &read_fds); // 添加监听fd

        // 步骤2：添加所有活跃客户端fd到读集合
        max_fd = listen_fd; // 重置max_fd
        for (size_t i = 0; i < client_list->capacity; i++)
        {
            int ctrl_fd = client_list->data[i].ctrl_fd;
            if (ctrl_fd >= 0 && ctrl_fd < FD_SETSIZE)
            {
                FD_SET(ctrl_fd, &read_fds);
                if (ctrl_fd > max_fd)
                {
                    max_fd = ctrl_fd; // 更新最大fd
                }
            }
            else if (ctrl_fd >= FD_SETSIZE)
            {
                // 超过可被 select 支持的范围，安全移除此连接
                fprintf(stderr, "client fd %d >= FD_SETSIZE (%d), removing it\n", ctrl_fd, FD_SETSIZE);
                client_conn_list_remove(client_list, ctrl_fd);
            }
        }
        // 步骤3：等待IO事件（超时1秒，避免永久阻塞）
        struct timeval tv = {1, 0};
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, &tv);
        if (activity < 0 && errno != EINTR)
        {
            perror("select error");
            continue;
        }

        // 步骤4：处理新连接请求
        if (FD_ISSET(listen_fd, &read_fds))
        {
            // 检查是否超过最大连接数限制
            if (client_list->used >= config->max_conn)
            {
                char client_ip[INET_ADDRSTRLEN];
                int client_port;
                int ctrl_fd = socket_accept(listen_fd, client_ip, &client_port);
                if (ctrl_fd != -1)
                {
                    socket_send(ctrl_fd, "421 Too many connections");
                    socket_close(ctrl_fd);
                    printf("Rejected connection from %s:%d (max conn: %d)\n",
                           client_ip, client_port, config->max_conn);
                }
                continue;
            }

            // 接受新连接并初始化
            char client_ip[INET_ADDRSTRLEN];
            int client_port;
            int ctrl_fd = socket_accept(listen_fd, client_ip, &client_port);
            if (ctrl_fd == -1)
            {
                perror("socket_accept failed");
                continue;
            }

            // 立即检查 fd 是否可被 select 使用
            if (ctrl_fd < 0 || ctrl_fd >= FD_SETSIZE)
            {
                // 不在 select 可接受范围内，拒绝连接并关闭
                socket_send(ctrl_fd, "421 Too many file descriptors");
                socket_close(ctrl_fd);
                fprintf(stderr, "accepted fd %d out of select range [0,%d), closed\n", ctrl_fd, FD_SETSIZE);
                continue;
            }

            ClientConn new_conn;
            client_conn_init(&new_conn, ctrl_fd, config->root_dir);
            // 添加到动态列表
            if (client_conn_list_add(client_list, &new_conn) != 0)
            {
                socket_send(ctrl_fd, "421 Server internal error");
                socket_close(ctrl_fd);
                continue;
            }

            // 发送欢迎信息
            socket_send(ctrl_fd, "220 Anonymous FTP server ready.\r\n");
            printf("New connection from %s:%d (used: %zu/%zu)\n",
                   client_ip, client_port, client_list->used, client_list->capacity);
        }

        // 步骤5：处理已连接客户端的命令
        for (size_t i = 0; i < client_list->capacity; i++)
        {
            int ctrl_fd = client_list->data[i].ctrl_fd;
            if (ctrl_fd == -1)
                continue; // 跳过空闲槽位

            if (ctrl_fd >= 0 && ctrl_fd < FD_SETSIZE && FD_ISSET(ctrl_fd, &read_fds))
            {
                int should_remove = handle_client_cmd(&client_list->data[i]);
                if (should_remove)
                {
                    // 使用之前保存的 ctrl_fd 调用移除（会关闭 fd 并回收槽位）
                    client_conn_list_remove(client_list, ctrl_fd);
                    break; // 退出循环，避免索引混乱
                }
            }
        }
    }
    // 步骤6：销毁动态列表（实际服务器不会执行到这里，除非退出循环）
    client_conn_list_destroy(client_list);
    socket_close(listen_fd);
}