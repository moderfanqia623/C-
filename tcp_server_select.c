#include <pthread.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define MAX_CONNECTIONS 5

int initTCPServer(unsigned short port) {
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
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        return -1;
    }

    char *endptr;
    unsigned short port = strtol(argv[1], &endptr, 10);
    int sock_fd = initTCPServer(port);

    char buf[128] = {0};
    int max_fd = sock_fd;
    fd_set rfds_bk; FD_ZERO(&rfds_bk);
    fd_set rfds; FD_ZERO(&rfds);
    FD_SET(sock_fd, &rfds_bk);
    int flag = 0;

    while (1) {
        rfds = rfds_bk;
        int ret = select(max_fd + 1, &rfds, NULL, NULL, NULL);
        printf("ret: %d\n", ret);
        for (int i=0; i<max_fd + 1; i++) {
            if (FD_ISSET(i, &rfds)) {   // 监听描述符可读，有新的连接就绪
                if (i == sock_fd) {
                    int new_fd = accept(i, NULL, NULL);
                    if (new_fd < 0) {
                        perror("accept");
                        flag = 1;
                        break;
                    }
                    printf("have a new connect %d\n", new_fd);
                    max_fd = max_fd > new_fd ? max_fd : new_fd;
                    FD_SET(new_fd, &rfds_bk);
                } else {
                    ret = recv(i, buf, sizeof(buf) - 1, 0);
                    if (ret <= 0) {
                        printf("remote client %d close\n", i);
                        FD_CLR(i, &rfds_bk);
                        close(i);
                        continue;
                    }
                    buf[ret] = 0;
                    struct sockaddr_in dest;
                    memset(&dest, 0, sizeof(dest));
                    socklen_t dest_len = sizeof(dest);
                    getpeername(i, (struct sockaddr *)&dest, &dest_len);
                    printf("ip:%s:%d, recv:%s\n", inet_ntoa(dest.sin_addr), ntohs(dest.sin_port), buf);
                }
            }
        }
        if (flag) break;
    }
    close(sock_fd);
    return 0;
}