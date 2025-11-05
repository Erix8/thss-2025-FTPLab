#include "client_transfer.h"
#include "../net/client_socket.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

/**
 * 从数据连接接收文件并保存到本地
 * @param data_fd 数据连接文件描述符
 * @param local_filename 本地保存的文件名
 * @return 0表示成功，非0表示失败
 */
int transfer_recv_file(int data_fd, const char *local_filename)
{
    if (data_fd < 0 || local_filename == NULL)
    {
        fprintf(stderr, "transfer_recv_file: invalid arguments\n");
        return -1;
    }

    // 以二进制写入模式创建/截断本地文件
    int out_fd = open(local_filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (out_fd < 0)
    {
        perror("open local file for write failed");
        return -1;
    }

    char buf[64 * 1024]; // 64KB缓冲区
    for (;;)
    {
        ssize_t n = recv(data_fd, buf, sizeof(buf), 0);
        if (n == 0)
        {
            // 对端正常关闭，传输完成
            break;
        }
        if (n < 0)
        {
            if (errno == EINTR)
                continue; // 被信号中断，重试
            perror("recv data failed");
            close(out_fd);
            return -1;
        }

        // 处理可能的部分写
        ssize_t total_written = 0;
        while (total_written < n)
        {
            ssize_t w = write(out_fd, buf + total_written, (size_t)(n - total_written));
            if (w < 0)
            {
                if (errno == EINTR)
                    continue;
                perror("write to local file failed");
                close(out_fd);
                return -1;
            }
            total_written += w;
        }
    }

    close(out_fd);
    return 0;
}

/**
 * 向数据连接发送本地文件（上传）
 * @param data_fd 数据连接文件描述符
 * @param local_filename 本地要上传的文件名
 * @return 0表示成功，非0表示失败
 */
int transfer_send_file(int data_fd, const char *local_filename)
{
    if (data_fd < 0 || local_filename == NULL)
    {
        fprintf(stderr, "transfer_send_file: invalid arguments\n");
        return -1;
    }

    // 以二进制只读打开本地文件
    int in_fd = open(local_filename, O_RDONLY);
    if (in_fd < 0)
    {
        perror("open local file for read failed");
        return -1;
    }

    char buf[64 * 1024];
    for (;;)
    {
        ssize_t n = read(in_fd, buf, sizeof(buf));
        if (n == 0)
        {
            // 文件读取完毕
            break;
        }
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            perror("read local file failed");
            close(in_fd);
            return -1;
        }

        // 处理可能的部分发送
        ssize_t total_sent = 0;
        while (total_sent < n)
        {
            ssize_t s = send(data_fd, buf + total_sent, (size_t)(n - total_sent), 0);
            if (s < 0)
            {
                if (errno == EINTR)
                    continue;
                perror("send data failed");
                close(in_fd);
                return -1;
            }
            total_sent += s;
        }
    }

    close(in_fd);
    return 0;
}

/**
 * 关闭数据连接
 * @param data_fd 要关闭的数据连接文件描述符
 */
void transfer_close_data_conn(int data_fd)
{
    if (data_fd >= 0)
    {
        close(data_fd);
    }
}