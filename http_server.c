#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define PORT 8080
#define BUFFER_SIZE 4096
#define MAX_CLIENTS 10

int initTCPServer(const unsigned short port) {
    int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_socket < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in self;
    memset(&self, 0, sizeof(self));
    self.sin_family = AF_INET;
    self.sin_port = htons(port);
    self.sin_addr.s_addr = htonl(INADDR_ANY);

    const int ret = bind(tcp_socket, (struct sockaddr*)&self, sizeof(self));
    if (ret < 0) {
        perror("bind");
        close(tcp_socket);
        return -1;
    }
    printf("bind success\n");

    listen(tcp_socket, 5);
    printf("listen...\n");

    return tcp_socket;
}

int main() {
    // 初始化服务器
    int server_fd = initTCPServer(PORT);
    if (server_fd < 0) {
        return -1;
    }

    char buf[BUFFER_SIZE] = {0};
    int max_fd = server_fd;
    fd_set rfds_bk;
    FD_ZERO(&rfds_bk);
    FD_SET(server_fd, &rfds_bk);

    // 存储客户端socket
    int client_sockets[MAX_CLIENTS] = {0};

    printf("Server started on port %d\n", PORT);

    while (1) {
        fd_set rfds = rfds_bk;
        int ret = select(max_fd + 1, &rfds, NULL, NULL, NULL);

        if (ret < 0) {
            perror("select");
            break;
        }

        // printf("select returned: %d\n", ret);

        for (int i = 0; i < max_fd + 1; i++) {
            if (FD_ISSET(i, &rfds)) {
                if (i == server_fd) {
                    // 处理新连接
                    struct sockaddr_in client_addr;
                    socklen_t addr_len = sizeof(client_addr);
                    int new_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
                    if (new_fd < 0) {
                        perror("accept");
                        continue;
                    }
                    printf("New connection from %s:%d, fd: %d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), new_fd);

                    // 添加到select监控集合
                    FD_SET(new_fd, &rfds_bk);
                    if (new_fd > max_fd) {
                        max_fd = new_fd;
                    }

                    // 存储客户端socket
                    for (int j = 0; j < MAX_CLIENTS; j++) {
                        if (client_sockets[j] == 0) {
                            client_sockets[j] = new_fd;
                            break;
                        }
                    }
                } else {
                    // 处理客户端数据
                    int bytes_read = read(i, buf, BUFFER_SIZE - 1);
                    if (bytes_read <= 0) {
                        // 客户端断开连接
                        printf("Client %d disconnected\n", i);
                        close(i);
                        FD_CLR(i, &rfds_bk);

                        // 从客户端数组中移除
                        for (int j = 0; j < MAX_CLIENTS; j++) {
                            if (client_sockets[j] == i) {
                                client_sockets[j] = 0;
                                break;
                            }
                        }
                    }
                    // 处理客户端请求
                    buf[bytes_read] = '\0';
                    printf("Request from client %d:\n%s\n", i, buf);

                    // 发送HTTP响应
                    char *response = "HTTP/1.1 200 OK\r\n"
                                     "Content-Type: text/plain\r\n"
                                     "Content-Length: 12\r\n"
                                     "\r\n"
                                     "I am the storm that is approaching!!!";
                    write(i, response, strlen(response));
                    printf("Response sent to client %d\n", i);

                    // 清空缓冲区
                    memset(buf, 0, BUFFER_SIZE);
                }
            }
        }
    }

    // 清理资源
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] > 0) {
            close(client_sockets[i]);
        }
    }
    close(server_fd);

    return 0;
}