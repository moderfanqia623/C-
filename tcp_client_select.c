#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netdb.h>

int initTCPClient(char *dest_ip, int dest_port) {
    // sock_fd
    int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_socket < 0) {
        perror("socket");
        return -1;
    }
    // connect
    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(dest_port);
    dest.sin_addr.s_addr = inet_addr(dest_ip);

    int ret = connect(tcp_socket, (struct sockaddr*)&dest, sizeof(dest));
    if (ret < 0) {
        perror("connect");
        return -1;
    }
    printf("connect success\n");

    return tcp_socket;
}

int main(int argc, char *argv[]) {

    if (argc != 3) {
        fprintf(stderr, "Usage: ./tcp_client_select <dest_ip> <dest_port>\n");
        return -1;
    }

    // 初始化客户端
    char *end_ptr;
    char *dest_ip = argv[1];
    int dest_port = strtol(argv[2], &end_ptr, 10);
    int sock_fd = initTCPClient(dest_ip, dest_port);
    if (sock_fd < 0) {
        perror("initTCPClient error");
        return -1;
    }

    fd_set readfds;         // 申请一个 select 集合
    FD_ZERO(&readfds);      // 清理这个集合
    // fd 0     fd 3
    FD_SET(0, &readfds);    // 把要监听的文件描述符添加到这个集合当中
    FD_SET(sock_fd, &readfds);
    char buf[256] = {0};
    int flag = 0;

    while (1) {
        // 每次循环重新设置监听集合（因为select会修改它）
        FD_SET(STDIN_FILENO, &readfds);     // STDIN_FILENO 的值就是 0
        FD_SET(sock_fd, &readfds);

        // ret: select返回值，表示就绪的文件描述符数量
        int ret = select(sock_fd + 1, &readfds, NULL, NULL, NULL);
        if (ret < 0) {
            perror("select");
            return -1;
        }
        printf("select return:%d\n", ret);

        // 遍历集合 看那个文件描述符 已经就绪
        for (int i=0; i<sock_fd + 1; i++) {
            if (FD_ISSET(i, &readfds)) {
                // 表明文件描述符 i 就绪了
                if (i == 0) {   // 标准输入就绪
                    fgets(buf, sizeof(buf), stdin);
                    send(sock_fd, buf, strlen(buf) - 1, 0);
                } else if (i == sock_fd) {  // socket 就绪
                    ret = recv(sock_fd, buf, sizeof(buf) - 1, 0);
                    if (ret <= 0) {         // 连接关闭或错误
                        printf("close...\n");
                        flag = 1;
                        break;
                    }
                    buf[ret] = 0;
                    printf("<recv:>%s\n", buf);
                }
            }
        }
        if (flag) break;
    }
    close(sock_fd);
    return 0;
}