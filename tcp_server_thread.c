#include <pthread.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 7789   // ����˶˿�
#define MAX_CONNECTIONS 3   // Ϊ��δ��� accept ���½����� TCP ���ӷ���Ļ�����е���󳤶�

/**
 * 1. socket ---> tcp_socket
 * 2. bind  ָ��������IP�� ���ĸ��˿�
 * 3. listen()  ���ü����˿�
 * 4. accept() �õ������ӵ��ļ�������
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

        // ��ͻ��˷��ͷ�����Ϣ
        char send_buf[1024] = {0};
        snprintf(send_buf, sizeof(send_buf), "Server received: %s", buf);
        send(new_fd, send_buf, strlen(send_buf), 0);
    }
    close(new_fd);
    return 0;
}

int main() {

    // ��ʼ�������
    unsigned short port = PORT;
    int tcp_socket = initTCPServer(port);
    if (tcp_socket < 0) {
        perror("initTCPServer");
        return -1;
    }

    // ���ڱ����������ӵĿͻ��˵ĵ�ַ��Ϣ
    // struct sockaddr_in dest = {0};
    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    unsigned int dest_len = sizeof(dest);
    char buf[1024] = {0};
    pthread_t tid;
    while (1) {
        // ͨ�� accept �õ�һ�������ӻ���� fd
        int new_fd = accept(tcp_socket, (struct sockaddr *) &dest, &dest_len);
        // int new_fd = accept(listen_fd, NULL, NULL);  // �����ؿͻ��˵ĵ�ַ��Ϣ��ֻ�������ӽ�������
        if (new_fd < 0) {
            perror("accept");
            break;
        }
        printf("New connection --> ip:%s port:%d\n", inet_ntoa(dest.sin_addr), ntohs(dest.sin_port));

        // �Կͻ������ӵ�ҵ����
        pthread_create(&tid, NULL, (void*(*)(void*)) &thread_handler, (void *) new_fd);
        // ���߳�����Ϊ����״̬
        // �߳�����ֹʱ���Զ��ͷ�������Դ��������Ҫ�����߳�ͨ��pthread_join������
        pthread_detach(tid);
    }
    close(tcp_socket);

    return 0;
}
