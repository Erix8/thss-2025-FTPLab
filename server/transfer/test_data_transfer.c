#include "data_transfer.h"
#include "../../utils/utils.h"
#include "../conn/client_conn.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

typedef struct
{
    int listen_fd;
    char recv_buf[64 * 1024];
    ssize_t recv_len;
} accept_read_ctx_t;

typedef struct
{
    int port;
    char recv_buf[64 * 1024];
    ssize_t recv_len;
} connect_read_ctx_t;

typedef struct
{
    int port;
    const char *send_buf;
    size_t send_len;
} connect_write_ctx_t;

static int make_loopback_listener(int *out_fd, int *out_port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;

    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1
    sa.sin_port = htons(0);                      // 任意端口

    if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) != 0)
    {
        close(fd);
        return -1;
    }
    if (listen(fd, 1) != 0)
    {
        close(fd);
        return -1;
    }

    struct sockaddr_in bound;
    socklen_t blen = sizeof(bound);
    if (getsockname(fd, (struct sockaddr *)&bound, &blen) != 0)
    {
        close(fd);
        return -1;
    }

    *out_fd = fd;
    *out_port = ntohs(bound.sin_port);
    return 0;
}

static void *thr_accept_and_read(void *arg)
{
    accept_read_ctx_t *ctx = (accept_read_ctx_t *)arg;
    int cfd = accept(ctx->listen_fd, NULL, NULL);
    if (cfd < 0)
    {
        ctx->recv_len = -1;
        return NULL;
    }
    close(ctx->listen_fd);
    ctx->listen_fd = -1;

    ssize_t total = 0;
    for (;;)
    {
        ssize_t r = read(cfd, ctx->recv_buf + total, sizeof(ctx->recv_buf) - total);
        if (r == 0)
            break;
        if (r < 0)
        {
            total = -1;
            break;
        }
        total += r;
        if ((size_t)total >= sizeof(ctx->recv_buf))
            break; // 截断
    }
    close(cfd);
    ctx->recv_len = total;
    return NULL;
}

static void *thr_connect_and_read(void *arg)
{
    connect_read_ctx_t *ctx = (connect_read_ctx_t *)arg;

    int cfd = socket(AF_INET, SOCK_STREAM, 0);
    if (cfd < 0)
    {
        ctx->recv_len = -1;
        return NULL;
    }

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons((uint16_t)ctx->port);
    inet_pton(AF_INET, "127.0.0.1", &sa.sin_addr);

    if (connect(cfd, (struct sockaddr *)&sa, sizeof(sa)) != 0)
    {
        close(cfd);
        ctx->recv_len = -1;
        return NULL;
    }

    ssize_t total = 0;
    for (;;)
    {
        ssize_t r = read(cfd, ctx->recv_buf + total, sizeof(ctx->recv_buf) - total);
        if (r == 0)
            break;
        if (r < 0)
        {
            total = -1;
            break;
        }
        total += r;
        if ((size_t)total >= sizeof(ctx->recv_buf))
            break;
    }
    close(cfd);
    ctx->recv_len = total;
    return NULL;
}

static void *thr_connect_and_write(void *arg)
{
    connect_write_ctx_t *ctx = (connect_write_ctx_t *)arg;

    int cfd = socket(AF_INET, SOCK_STREAM, 0);
    if (cfd < 0)
        return NULL;

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons((uint16_t)ctx->port);
    inet_pton(AF_INET, "127.0.0.1", &sa.sin_addr);

    if (connect(cfd, (struct sockaddr *)&sa, sizeof(sa)) != 0)
    {
        close(cfd);
        return NULL;
    }

    const char *p = ctx->send_buf;
    size_t left = ctx->send_len;
    while (left > 0)
    {
        ssize_t w = write(cfd, p, left);
        if (w <= 0)
            break;
        p += w;
        left -= (size_t)w;
    }
    shutdown(cfd, SHUT_WR);
    close(cfd);
    return NULL;
}

static void init_conn(ClientConn *conn, const char *workdir)
{
    memset(conn, 0, sizeof(*conn));
    conn->ctrl_fd = -1;
    conn->data_fd = -1;
    conn->data_mode = DATA_MODE_NONE;
    conn->pasv_listen_fd = -1;
    conn->data_host[0] = '\0';
    conn->data_port = 0;
    strncpy(conn->current_dir, workdir, sizeof(conn->current_dir) - 1);
    conn->current_dir[sizeof(conn->current_dir) - 1] = '\0';
}

static char *make_temp_dir(char *buf, size_t len)
{
    snprintf(buf, len, "/tmp/ftp_transfer_test_XXXXXX");
    return mkdtemp(buf);
}

static int write_file(const char *dir, const char *name, const char *data)
{
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
        return -1;
    size_t n = strlen(data);
    const char *p = data;
    while (n > 0)
    {
        ssize_t w = write(fd, p, n);
        if (w <= 0)
        {
            close(fd);
            return -1;
        }
        p += w;
        n -= (size_t)w;
    }
    close(fd);
    return 0;
}

static int read_file(const char *dir, const char *name, char *out, size_t out_len)
{
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return -1;
    ssize_t total = 0;
    for (;;)
    {
        ssize_t r = read(fd, out + total, (out_len > (size_t)total ? out_len - (size_t)total : 0));
        if (r == 0)
            break;
        if (r < 0)
        {
            total = -1;
            break;
        }
        total += r;
        if ((size_t)total >= out_len)
            break;
    }
    close(fd);
    return (int)total;
}

static void test_port_list()
{
    printf("== test_port_list (PORT + transfer_send_list) ==\n");

    char tmpdir[PATH_MAX];
    if (!make_temp_dir(tmpdir, sizeof(tmpdir)))
    {
        printf("mkdtemp failed\n");
        return;
    }
    // 准备两个文件用于 LIST
    write_file(tmpdir, "a.txt", "");
    write_file(tmpdir, "b.bin", "");

    int lfd, lport;
    if (make_loopback_listener(&lfd, &lport) != 0)
    {
        printf("listener create failed\n");
        return;
    }

    accept_read_ctx_t ar = {.listen_fd = lfd, .recv_len = 0};
    pthread_t th;
    pthread_create(&th, NULL, thr_accept_and_read, &ar);

    ClientConn conn;
    init_conn(&conn, tmpdir);
    conn.data_mode = DATA_MODE_PORT;
    strncpy(conn.data_host, "127.0.0.1", sizeof(conn.data_host) - 1);
    conn.data_port = (uint16_t)lport;

    if (transfer_init_data_conn(&conn) != 0)
    {
        printf("transfer_init_data_conn failed (PORT)\n");
        return;
    }
    if (transfer_send_list(&conn) != 0)
    {
        printf("transfer_send_list failed\n");
    }
    transfer_close_data_conn(&conn);

    pthread_join(th, NULL);
    if (ar.recv_len > 0)
    {
        printf("LIST received (%zd bytes):\n%.*s", ar.recv_len, (int)ar.recv_len, ar.recv_buf);
    }
    else
    {
        printf("LIST receive failed\n");
    }
}

static void test_pasv_send_file()
{
    printf("== test_pasv_send_file (PASV + transfer_send_file) ==\n");

    char tmpdir[PATH_MAX];
    if (!make_temp_dir(tmpdir, sizeof(tmpdir)))
    {
        printf("mkdtemp failed\n");
        return;
    }
    const char *fname = "hello.txt";
    const char *content = "Hello Passive Mode!\n";
    write_file(tmpdir, fname, content);

    int lfd, lport;
    if (make_loopback_listener(&lfd, &lport) != 0)
    {
        printf("listener create failed\n");
        return;
    }

    connect_read_ctx_t cr = {.port = lport, .recv_len = 0};
    pthread_t th;
    pthread_create(&th, NULL, thr_connect_and_read, &cr);

    ClientConn conn;
    init_conn(&conn, tmpdir);
    conn.data_mode = DATA_MODE_PASV;
    conn.pasv_listen_fd = lfd;

    if (transfer_init_data_conn(&conn) != 0)
    {
        printf("transfer_init_data_conn failed (PASV)\n");
        return;
    }

    if (transfer_send_file(&conn, fname) != 0)
    {
        printf("transfer_send_file failed\n");
    }
    transfer_close_data_conn(&conn);

    pthread_join(th, NULL);
    if (cr.recv_len > 0)
    {
        printf("RETR received (%zd bytes), begins with: %.*s", cr.recv_len,
               (int)((cr.recv_len > 40) ? 40 : cr.recv_len), cr.recv_buf);
    }
    else
    {
        printf("RETR receive failed\n");
    }
}

static void test_pasv_recv_file()
{
    printf("== test_pasv_recv_file (PASV + transfer_recv_file) ==\n");

    char tmpdir[PATH_MAX];
    if (!make_temp_dir(tmpdir, sizeof(tmpdir)))
    {
        printf("mkdtemp failed\n");
        return;
    }

    int lfd, lport;
    if (make_loopback_listener(&lfd, &lport) != 0)
    {
        printf("listener create failed\n");
        return;
    }

    const char *send_data = "Upload from client to server via PASV.\n";
    connect_write_ctx_t cw = {.port = lport, .send_buf = send_data, .send_len = strlen(send_data)};
    pthread_t th;
    pthread_create(&th, NULL, thr_connect_and_write, &cw);

    ClientConn conn;
    init_conn(&conn, tmpdir);
    conn.data_mode = DATA_MODE_PASV;
    conn.pasv_listen_fd = lfd;

    if (transfer_init_data_conn(&conn) != 0)
    {
        printf("transfer_init_data_conn failed (PASV)\n");
        return;
    }
    const char *out_name = "upload.txt";
    if (transfer_recv_file(&conn, out_name) != 0)
    {
        printf("transfer_recv_file failed\n");
    }
    transfer_close_data_conn(&conn);

    pthread_join(th, NULL);

    char buf[4096] = {0};
    int n = read_file(tmpdir, out_name, buf, sizeof(buf));
    if (n > 0)
    {
        printf("STOR saved (%d bytes), begins with: %.*s", n, (n > 40 ? 40 : n), buf);
    }
    else
    {
        printf("STOR file read failed\n");
    }
}

int main()
{
    test_port_list();
    test_pasv_send_file();
    test_pasv_recv_file();
    return 0;
}