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
#include <sys/types.h>

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

/**
 * 接收目录列表（LIST/NLST），根据需要将 CRLF 转为 \n，并输出到 stdout 或文件
 * @param data_fd 数据连接文件描述符
 * @param local_filename 若为NULL则输出到标准输出，否则写入该文件
 * @param normalize_crlf 非0则将"\r\n"标准化为"\n"，并忽略孤立的'\r'
 * @return 0表示成功，非0表示失败
 */
int transfer_recv_list(int data_fd, const char *local_filename, int normalize_crlf)
{
    if (data_fd < 0)
    {
        fprintf(stderr, "transfer_recv_list: invalid data fd\n");
        return -1;
    }

    int out_fd = -1;
    int need_close = 0;
    if (local_filename == NULL)
    {
        out_fd = STDOUT_FILENO; // 打印到终端
    }
    else
    {
        out_fd = open(local_filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (out_fd < 0)
        {
            perror("open list output file failed");
            return -1;
        }
        need_close = 1;
    }

    char inbuf[64 * 1024];
    char outbuf[64 * 1024 * 2]; // 预留CRLF->LF变化，虽然总体不会增大，但给足空间
    int prev_cr = 0;            // 是否上一个字符为'\r'

    for (;;)
    {
        ssize_t n = recv(data_fd, inbuf, sizeof(inbuf), 0);
        if (n == 0)
            break; // 对端关闭
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            perror("recv list data failed");
            if (need_close)
                close(out_fd);
            return -1;
        }

        size_t o = 0;
        if (normalize_crlf)
        {
            for (ssize_t i = 0; i < n; ++i)
            {
                unsigned char c = (unsigned char)inbuf[i];
                if (prev_cr)
                {
                    if (c == '\n')
                    {
                        // 将 CRLF 合并为 \n
                        outbuf[o++] = '\n';
                        prev_cr = 0;
                        continue;
                    }
                    else
                    {
                        // 孤立的'\r'，按换行处理
                        outbuf[o++] = '\n';
                        // 继续处理当前字符c
                        prev_cr = 0;
                    }
                }

                if (c == '\r')
                {
                    prev_cr = 1;
                }
                else
                {
                    outbuf[o++] = (char)c;
                }
            }
        }
        else
        {
            // 不转换，直接输出
            memcpy(outbuf, inbuf, (size_t)n);
            o = (size_t)n;
        }

        // 如果最后一个是悬挂的'\r'，先不输出，等下一个字符判断
        // flush本批次处理好的输出
        size_t left = o;
        size_t off = 0;
        while (left > 0)
        {
            ssize_t w = write(out_fd, outbuf + off, left);
            if (w < 0)
            {
                if (errno == EINTR)
                    continue;
                perror("write list output failed");
                if (need_close)
                    close(out_fd);
                return -1;
            }
            off += (size_t)w;
            left -= (size_t)w;
        }
    }

    // 处理最后一个悬挂的'\r'
    if (normalize_crlf && prev_cr)
    {
        const char nl = '\n';
        if (write(out_fd, &nl, 1) < 0)
        {
            perror("write list trailing newline failed");
            if (need_close)
                close(out_fd);
            return -1;
        }
    }

    if (need_close)
        close(out_fd);
    return 0;
}