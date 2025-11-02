#include "data_transfer.h"
#include "../../utils/utils.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

// 内部：打开主动模式数据连接（PORT）
static int open_active(ClientConn *conn)
{
    if (!conn || conn->data_mode != DATA_MODE_PORT)
        return -1;
    if (conn->data_host[0] == '\0' || conn->data_port == 0)
        return -1;

    if (conn->data_fd >= 0)
    {
        close(conn->data_fd);
        conn->data_fd = -1;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(conn->data_port);
    if (inet_pton(AF_INET, conn->data_host, &sa.sin_addr) != 1)
    {
        close(fd);
        return -1;
    }

    if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0)
    {
        close(fd);
        return -1;
    }

    conn->data_fd = fd;
    return 0;
}

// 内部：在被动模式下接受数据连接（阻塞或带轻微超时）
static int open_passive(ClientConn *conn, int timeout_ms)
{
    if (!conn || conn->data_mode != DATA_MODE_PASV || conn->pasv_listen_fd < 0)
        return -1;

    int lfd = conn->pasv_listen_fd;

    if (timeout_ms >= 0)
    {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(lfd, &rfds);
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        int rc = select(lfd + 1, &rfds, NULL, NULL, &tv);
        if (rc <= 0) // 0超时，<0出错
            return -1;
    }

    int dfd = accept(lfd, NULL, NULL);
    if (dfd < 0)
        return -1;

    // 接受一个连接后，典型实现就关闭监听
    close(lfd);
    conn->pasv_listen_fd = -1;
    conn->data_fd = dfd;
    return 0;
}

/**
 * 初始化数据连接（根据客户端连接的data_mode）
 * @param conn 客户端连接结构体指针
 * @return 0表示成功，-1表示失败
 */
int transfer_init_data_conn(ClientConn *conn)
{
    if (!conn)
        return -1;

    // 如果已有上一次的数据连接，先关闭
    if (conn->data_fd >= 0)
    {
        close(conn->data_fd);
        conn->data_fd = -1;
    }

    if (conn->data_mode == DATA_MODE_PORT)
    {
        return open_active(conn);
    }
    else if (conn->data_mode == DATA_MODE_PASV)
    {
        // 设一个较短超时（例如5秒）
        return open_passive(conn, 5000);
    }

    return -1;
}

/**
 * 通过数据连接向客户端发送文件
 * @param conn 客户端连接结构体指针
 * @param filename 要发送的文件名（基于服务器当前目录）
 * @return 0表示成功，-1表示失败
 */
int transfer_send_file(ClientConn *conn, const char *filename)
{
    if (!conn || conn->data_fd < 0 || !filename)
        return -1;

    char path[PATH_MAX];
    // 将 filename 拼到当前目录（限制在当前目录下）
    if (!utils_join_path(conn->current_dir, filename, path, sizeof(path)))
        return -1;

    if (!utils_check_path(conn->root_dir, path))
        return -1;

    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return -1;

    char buf[16 * 1024];
    for (;;)
    {
        ssize_t r = read(fd, buf, sizeof(buf));
        if (r == 0)
            break;
        if (r < 0)
        {
            close(fd);
            return -1;
        }

        const char *p = buf;
        ssize_t left = r;
        while (left > 0)
        {
            ssize_t w = write(conn->data_fd, p, (size_t)left);
            if (w <= 0)
            {
                close(fd);
                return -1;
            }
            p += w;
            left -= w;
        }
    }

    close(fd);
    return 0;
}

/**
 * 通过数据连接接收客户端上传的文件
 * @param conn 客户端连接结构体指针
 * @param filename 要保存的文件名（基于服务器当前目录）
 * @return 0表示成功，-1表示失败
 */
int transfer_recv_file(ClientConn *conn, const char *filename)
{
    if (!conn || conn->data_fd < 0 || !filename)
        return -1;

    char path[PATH_MAX];
    if (!utils_join_path(conn->current_dir, filename, path, sizeof(path)))
        return -1;
    if (!utils_check_path(conn->root_dir, path))
        return -1;

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
        return -1;

    char buf[16 * 1024];
    for (;;)
    {
        ssize_t r = read(conn->data_fd, buf, sizeof(buf));
        if (r == 0)
            break;
        if (r < 0)
        {
            close(fd);
            return -1;
        }

        const char *p = buf;
        ssize_t left = r;
        while (left > 0)
        {
            ssize_t w = write(fd, p, (size_t)left);
            if (w <= 0)
            {
                close(fd);
                return -1;
            }
            p += w;
            left -= w;
        }
    }

    close(fd);
    return 0;
}

/**
 * 通过数据连接向客户端发送目录列表
 * @param conn 客户端连接结构体指针
 * @return 0表示成功，-1表示失败
 */
int transfer_send_list(ClientConn *conn)
{
    if (!conn || conn->data_fd < 0)
        return -1;

    // 防御性校验：当前工作目录必须在根内
    if (!utils_check_path(conn->root_dir, conn->current_dir))
        return -1;

    // 通过子进程执行 /bin/ls -lA -- <current_dir>，父进程把输出转为 CRLF 并转发到数据连接
    int pipefd[2];
    if (pipe(pipefd) != 0)
        return -1;
    pid_t pid = fork();
    if (pid < 0)
    {
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }
    if (pid == 0)
    {
        // child: 将 stdout/err 重定向到管道
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        // 不改变进程工作目录，直接传递目录参数，避免影响其他线程
        execl("/bin/ls", "ls", "-lA", "--", conn->current_dir, (char *)NULL);
        // 若 exec 失败，退出
        _exit(127);
    }
    // parent
    close(pipefd[1]);
    char inbuf[8192];
    char outbuf[16384]; // 预留 CRLF 扩展
    ssize_t r;
    while ((r = read(pipefd[0], inbuf, sizeof(inbuf))) > 0)
    {
        // 将 '\n' 转换为 "\r\n"
        size_t oi = 0;
        for (ssize_t i = 0; i < r; ++i)
        {
            if ((size_t)oi + 2 >= sizeof(outbuf))
            {
                // flush
                ssize_t left = oi,
                        off = 0;
                while (left > 0)
                {
                    ssize_t w = write(conn->data_fd, outbuf + off, (size_t)left);
                    if (w <= 0)
                    {
                        close(pipefd[0]);
                        return -1;
                    }
                    off += w;
                    left -= w;
                }
                oi = 0;
            }
            if (inbuf[i] == '\n')
            {
                outbuf[oi++] = '\r';
                outbuf[oi++] = '\n';
            }
            else
            {
                outbuf[oi++] = inbuf[i];
            }
        }
        // flush remaining
        ssize_t left = oi,
                off = 0;
        while (left > 0)
        {
            ssize_t w = write(conn->data_fd, outbuf + off, (size_t)left);
            if (w <= 0)
            {
                close(pipefd[0]);
                return -1;
            }
            off += w;
            left -= w;
        }
    }
    close(pipefd[0]);
    // 等待子进程结束
    int status = 0;
    (void)waitpid(pid, &status, 0);
    return 0;
}

/**
 * 关闭客户端连接的数据连接
 * @param conn 客户端连接结构体指针
 */
void transfer_close_data_conn(ClientConn *conn)
{
    if (!conn)
        return;

    if (conn->data_fd >= 0)
    {
        close(conn->data_fd);
        conn->data_fd = -1;
    }
    // 一次传输后通常重置为NONE，要求下次传输前再发 PORT/PASV
    conn->data_mode = DATA_MODE_NONE;
    conn->data_host[0] = '\0';
    conn->data_port = 0;

    if (conn->pasv_listen_fd >= 0)
    {
        close(conn->pasv_listen_fd);
        conn->pasv_listen_fd = -1;
    }
}