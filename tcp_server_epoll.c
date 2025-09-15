#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/epoll.h>

#define MAX_CONNECTIONS 5

int initTCPServer(const unsigned short port) {

    int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_socket < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in self;
    self.sin_family = AF_INET;
    self.sin_port = htons(port);
    self.sin_addr.s_addr = htonl(INADDR_ANY);

    int ret = bind(tcp_socket, (struct sockaddr *)&self, sizeof(self));
    if (ret < 0) {
        perror("bind");
        return -1;
    }
    printf("bind success\n");

    listen(tcp_socket, MAX_CONNECTIONS);
    printf("listen...\n");

    return tcp_socket;
}

int main(int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "Usage: ./%s <server_port>", argv[0]);
        return -1;
    }

    char *endptr;
    const unsigned short port = strtol(argv[1], &endptr, 10);
    const int listen_fd = initTCPServer(port);
    if (listen_fd < 0) {
        perror("initTCPServer error");
        return -1;
    }

    // epoll for server
    // 创建epoll对象、添加fd、等待就绪
    int epfd = epoll_create(1);
    if (epfd < 0) {
        perror("epoll_create error");
        close(listen_fd);
        return -1;
    }

    struct epoll_event fd_info = {0};
    // EPOLLIN 表示“该描述符可读”，也就是有数据可读取时 epoll_wait 会触发该事件
    fd_info.events = EPOLLIN;
    fd_info.data.fd = STDIN_FILENO;
    epoll_ctl(epfd, EPOLL_CTL_ADD, STDIN_FILENO, &fd_info);

    fd_info.data.fd = listen_fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &fd_info);

    struct epoll_event evs[5];
    char buf[1024];
    int conn_fds[1024] = {0};   // 连接的客户端数量
    int flag = 0;
    while (1) {
        int nfds = epoll_wait(epfd, evs, sizeof(evs) / sizeof(evs[0]), -1);
        if (nfds < 0) {
            perror("epoll_wait");
            return -1;
        }
        printf("nfds:%d\n", nfds);

        for (int i=0; i<nfds; i++) {
            if (evs[i].data.fd == STDIN_FILENO) {   // fd 0
                fgets(buf, sizeof(buf), stdin);
                if (strncmp(buf, "list", 4) == 0) {     // 打印当前在线的客户端
                    for (int j=0; j<sizeof(conn_fds)/sizeof(conn_fds[0]); j++) {
                        if (conn_fds[j] == 1) {
                            printf("client %d online\n", j);
                        }
                    }
                }
                if (strncmp(buf, "quit", 4) == 0) {     // 提出所有在线的客户端并关闭服务器
                    printf("closing server\n");
                    for (int j=0; j<sizeof(conn_fds)/sizeof(conn_fds[0]); j++) {
                        if (conn_fds[j] == 1) {
                            epoll_ctl(epfd, EPOLL_CTL_DEL, j, NULL);
                            printf("client %d offline\n", j);
                        }
                    }
                    flag = 1;
                    break;
                }
            } else if (evs[i].data.fd == listen_fd) {
                int new_fd = accept(listen_fd, NULL, NULL);
                if (new_fd < 0) {
                    perror("accept");
                    flag = 1;
                    break;
                }
                printf("have a new connect: %d\n", new_fd);
                fd_info.events = EPOLLIN;
                fd_info.data.fd = new_fd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, new_fd, &fd_info);
                conn_fds[new_fd] = 1;
            } else {
                int conn_fd = evs[i].data.fd;
                int ret = recv(evs[i].data.fd, buf, sizeof(buf) - 1, 0);
                if (ret <= 0) {
                    perror("recv");
                    epoll_ctl(epfd, EPOLL_CTL_DEL, evs[i].data.fd, NULL);
                    conn_fds[conn_fd] = 0;
                    close(evs[i].data.fd);
                    continue;
                }
                buf[ret] = '\0';
                printf("<recv:>%s\n", buf);
                // 向其他客户端转发消息
                printf("broadcasted...\n");
                for (int j=0; j<sizeof(conn_fds)/sizeof(conn_fds[0]); j++) {
                    if (conn_fds[j] == 1 && j != conn_fd) {
                        char client_info_buf[1024] = {0};
                        snprintf(client_info_buf, sizeof(client_info_buf), "<user %d>: ", conn_fd);
                        send(j, client_info_buf, strlen(client_info_buf), 0);
                        send(j, buf, strlen(buf), 0);
                    }
                }
            }
        }
        if (flag) {
            break;
        }
    }

    close(listen_fd);
    printf("server closed\n");
    return 0;
}