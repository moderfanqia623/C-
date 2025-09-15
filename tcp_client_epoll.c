#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/epoll.h>

int initTCPClient(const char *dest_ip, const char *dest_port) {
    int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_socket < 0) {
        perror("socket");
        return -1;
    }
    printf("tcp_socket is %d\n", tcp_socket);

    char *endptr;
    const unsigned short port = strtol(dest_port, &endptr, 10);
    struct sockaddr_in dest = {0};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);
    dest.sin_addr.s_addr = inet_addr(dest_ip);

    int ret = connect(tcp_socket, (struct sockaddr *)&dest, sizeof(dest));
    if (ret < 0) {
        perror("connect");
        return -1;
    }
    printf("connect success\n");
    return tcp_socket;
}

int main(int argc, char *argv[]) {

    if (argc != 3) {
        fprintf(stderr, "Usage: ./%s <dest_ip> <dest_port>", argv[0]);
        return -1;
    }

    const int conn_fd = initTCPClient(argv[1], argv[2]);
    if (conn_fd < 0) {
        fprintf(stderr, "initTCPClient error\n");
        return -1;
    }

    // epoll for client
    // 创建epoll对象、添加fd、等待就绪
    int epfd = epoll_create(1);
    if (epfd < 0) {
        perror("epoll_create");
        close(conn_fd);
        return -1;
    }
    // fd 0     fd 3 conn_fd
    struct epoll_event fd_info = {0};
    // EPOLLIN 表示“该描述符可读”，也就是有数据可读取时 epoll_wait 会触发该事件
    fd_info.events = EPOLLIN;
    fd_info.data.fd = STDIN_FILENO;
    epoll_ctl(epfd, EPOLL_CTL_ADD, STDIN_FILENO, &fd_info);

    fd_info.data.fd = conn_fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, conn_fd, &fd_info);

    struct epoll_event evs[5];  // 一次性最多处理就绪事件的数量：5（可变）
    char buf[1024];
    int flag = 0;
    while (1) {
        int nfds = epoll_wait(epfd, evs, sizeof(evs) / sizeof(evs[0]), -1);
        if (nfds < 0) {
            perror("epoll_wait");
            return -1;
        }
        printf("nfds:%d\n", nfds);

        for (int i=0; i<nfds; i++) {
            if (evs[i].data.fd == STDIN_FILENO) {
                // fd 0 --> fgets --> send
                fgets(buf, sizeof(buf), stdin);
                send(conn_fd, buf, strlen(buf) - 1, 0);
            } else if (evs[i].data.fd == conn_fd) {
                // recv
                int ret = recv(conn_fd, buf, sizeof(buf) - 1, 0);
                if (ret <= 0) {
                    perror("remote close");
                    flag = 1;
                    continue;
                }
                buf[ret] = 0;
                printf("<recv:>%s\n", buf);
            }
        }
        if (flag) {
            break;
        }
    }
    close(conn_fd);
    return 0;
}