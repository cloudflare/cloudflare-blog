#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int connect_unix()
{
    struct sockaddr_un addr = {.sun_family = AF_UNIX,
                               .sun_path = "/tmp/scm_example.sock"};

    int unix_sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (unix_sock == -1)
        return -1;

    if (connect(unix_sock, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        close(unix_sock);
        return -1;
    }

    return unix_sock;
}

int send_fd(int unix_sock, int fd)
{
    struct iovec iov = {.iov_base = ":)", // Must send at least one byte
                        .iov_len = 2};

    union {
        char buf[CMSG_SPACE(sizeof(fd))];
        struct cmsghdr align;
    } u;

    struct msghdr msg = {.msg_iov = &iov,
                         .msg_iovlen = 1,
                         .msg_control = u.buf,
                         .msg_controllen = sizeof(u.buf)};

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    *cmsg = (struct cmsghdr){.cmsg_level = SOL_SOCKET,
                             .cmsg_type = SCM_RIGHTS,
                             .cmsg_len = CMSG_LEN(sizeof(fd))};

    memcpy(CMSG_DATA(cmsg), &fd, sizeof(fd));

    return sendmsg(unix_sock, &msg, 0);
}

int main(int argc, char *argv[])
{
    struct sockaddr_in addr = {.sin_family = AF_INET,
                               .sin_port = htons(8001),
                               .sin_addr = (struct in_addr){.s_addr = INADDR_ANY}};

    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1)
    {
        fprintf(stderr, "TCP sock error: %s\n", strerror(errno));
        return -1;
    }

    if (bind(server_sock, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        fprintf(stderr, "TCP bind error: %s\n", strerror(errno));
        return -1;
    }

    if (listen(server_sock, 50) == -1)
    {
        fprintf(stderr, "TCP listen error: %s\n", strerror(errno));
        return -1;
    }

    while (1)
    {
        struct sockaddr_in remote_addr;
        socklen_t addr_len = sizeof(remote_addr);
        int tcp_conn = accept(server_sock, (struct sockaddr *)&remote_addr, &addr_len);

        char buf[4] = {0};
        ssize_t n = recv(tcp_conn, buf, 4, MSG_PEEK); // Peek to check the first bytes

        if (n == 4 && buf[0] == 'p' && buf[1] == 'a' && buf[2] == 's' && buf[3] == 's')
        {
            // If a message begins with pass, we send the connection to the other process
            int unix_sock = connect_unix();
            if (unix_sock == -1)
            {
                fprintf(stderr, "UNIX socket error: %s\n", strerror(errno));
                return -1;
            }

            int err = send_fd(unix_sock, tcp_conn);
            close(unix_sock);

            if (err == -1)
            {
                fprintf(stderr, "UNIX send error: %s\n", strerror(errno));
                return -1;
            }
        }
        else
        {
            // Otherwise we handle it
            char msg[] = "Hello from C!!!\n";
            if (send(tcp_conn, msg, sizeof(msg) - 1, 0) == -1)
            {
                fprintf(stderr, "TCP send error: %s\n", strerror(errno));
                return -1;
            }
        }

        close(tcp_conn);
    }
}