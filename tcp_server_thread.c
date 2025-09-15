#include <pthread.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 7789   // 服务端端口
#define MAX_CONNECTIONS 3   // 为尚未完成 accept 的新建立的 TCP 连接分配的缓冲队列的最大长度

/**
 * 1. socket ---> tcp_socket
 * 2. bind  指明监听的IP， 打开哪个端口
 * 3. listen()  设置监听端口
 * 4. accept() 拿到新连接的文件描述符
 * 5. recv / send ....
 */
int initTCPServer(unsigned short port) {

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

    int ret = bind(tcp_socket, (struct sockaddr *)&self, sizeof(self));
    if (ret < 0) {
        perror("bind");
        return -1;
    }
    printf("bind successful\n");

    listen(tcp_socket, MAX_CONNECTIONS);
    printf("listen...\n");

    return tcp_socket;
}

int thread_handler(const int new_fd) {

    char buf[1024];
    while (1) {
        int ret = recv(new_fd, buf, sizeof(buf), 0);
        if (ret < 0) {
            perror("recv");
            break;
        } else if (ret == 0) {
            printf("remote client close\n\n");
            break;
        }
        buf[ret] = '\0';
        printf("new_fd:%d   <recv:>%s\n", new_fd, buf);

        // 向客户端发送反馈消息
        char send_buf[1024] = {0};
        snprintf(send_buf, sizeof(send_buf), "Server received: %s", buf);
        send(new_fd, send_buf, strlen(send_buf), 0);
    }
    close(new_fd);
    return 0;
}

int main() {

    // 初始化服务端
    unsigned short port = PORT;
    int tcp_socket = initTCPServer(port);
    if (tcp_socket < 0) {
        perror("initTCPServer");
        return -1;
    }

    // 用于保存连已连接的客户端的地址信息
    // struct sockaddr_in dest = {0};
    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    unsigned int dest_len = sizeof(dest);
    char buf[1024] = {0};
    pthread_t tid;
    while (1) {
        // 通过 accept 拿到一个新连接缓存的 fd
        int new_fd = accept(tcp_socket, (struct sockaddr *) &dest, &dest_len);
        // int new_fd = accept(listen_fd, NULL, NULL);  // 不返回客户端的地址信息，只关心连接建立本身
        if (new_fd < 0) {
            perror("accept");
            break;
        }
        printf("New connection --> ip:%s port:%d\n", inet_ntoa(dest.sin_addr), ntohs(dest.sin_port));

        // 对客户端连接的业务处理
        pthread_create(&tid, NULL, (void*(*)(void*)) &thread_handler, (void *) new_fd);
        // 将线程设置为分离状态
        // 线程在终止时会自动释放所有资源，而不需要其他线程通过pthread_join来回收
        pthread_detach(tid);
    }
    close(tcp_socket);

    return 0;
}
