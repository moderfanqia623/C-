#include <pthread.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define DEST_IP "192.168.46.133"    // 目的服务端地址
#define DEST_PORT 7789              // 目的服务端端口

int main() {

    int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_socket < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in dest;
    dest.sin_family = AF_INET;
    dest.sin_port = htons(DEST_PORT);
    dest.sin_addr.s_addr = inet_addr(DEST_IP);

    int ret = connect(tcp_socket, (struct sockaddr*)&dest, sizeof(dest));
    if (ret < 0) {
        perror("connect");
        return -1;
    }
    printf("Server connected\n");

    char buf[1024] = {0};
    while (fgets(buf, sizeof(buf), stdin) != NULL) {
        if (strcmp(buf, "close\n") == 0) {
            printf("Disconnect\n");
            break;
        }
        send(tcp_socket, buf, strlen(buf), 0);
        ret = recv(tcp_socket, buf, sizeof(buf), 0);
        buf[ret] = '\0';
        printf("%s\n", buf);
    }
    close(tcp_socket);

    return 0;
}