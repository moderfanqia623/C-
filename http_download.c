#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

typedef struct {
    int tasock_fd_id;
    short port;                 // 端口
    char hostname[64];          // 主机名
    char res_path[256];         // 资源定位符
    char filename[64];          // 文件名
}tasock_fdInfo;

tasock_fdInfo *createTasock_fd(int id){
    // 申请一个 tasock_fdinfo 的空间
    tasock_fdInfo *tasock_fd = (tasock_fdInfo *)malloc(sizeof(tasock_fdInfo));
    if (tasock_fd == NULL) {
        fprintf(stderr, "malloc error: %s\n", strerror(errno));
        return NULL;
    }
    // 清理这个空间
    memset(tasock_fd, 0, sizeof(tasock_fdInfo));
    tasock_fd->tasock_fd_id = id;
    return tasock_fd;
}

void releaseTasock_fd(tasock_fdInfo *tasock_fd) {
    if (tasock_fd) {
        free(tasock_fd);
    }
}

int parser_tasock_fd(const char *url, tasock_fdInfo *tasock_fd) {
    char *p1;				// 遍历url的辅助指针
    char *s;				// 解析使用的尾部定位指针
    char *t;				// 找文件名的前置指针
    p1 = strstr(url, "http://");
    if (p1 == NULL) {
        fprintf(stderr, "No http protocol header!\n");
        return -1;
    }
    p1 += strlen("http://");
    tasock_fd->port = 80;
    // 匹配1153288396.rsc.cdn77.org/img/cdn77-test-563kb.jpg 里面的第一个 /
    // 拿到主机名 1153288396.rsc.cdn77.org
    s = strstr(p1, "/");
    if (s == NULL) {
        fprintf(stderr, "URL format invalid!\n");
        return -1;
    }
    *s = 0;
    // 1153288396.rsc.cdn77.org（0）img/cdn77-test-563kb.jpg
    // |
    // p1
    snprintf(tasock_fd->hostname, sizeof(tasock_fd->hostname), "%s", p1);
    *s = '/';
    // 1153288396.rsc.cdn77.org/img/cdn77-test-563kb.jpg
    //                         |
    //                         s
    snprintf(tasock_fd->res_path, sizeof(tasock_fd->res_path), "%s", s);
    // 解析文件名，从res_path中找到最后一个/
    t = s;
    while ((s = strstr(s + 1, "/")) != NULL) {
        t = s;
    }
    // 1153288396.rsc.cdn77.org/img/cdn77-test-563kb.jpg
    //                             |
    //                             t
    snprintf(tasock_fd->filename, sizeof(tasock_fd->filename), "%s", t + 1);
    return 0;
}

int init_socket(tasock_fdInfo *tasock_fd) {
    int sock_fd;
    struct sockaddr_in dest;
    int ret;

    // 申请一个 tcp_socket
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        return -1;
    }
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    // http   80
    dest.sin_port = htons(tasock_fd->port);
    // hostname ---> ip    --->dest.->ip
    struct hostent *pHostent = gethostbyname(tasock_fd->hostname);
    if (pHostent == NULL) {
        fprintf(stderr, "get host name %d: %s\n", h_errno, hstrerror(h_errno));
        return -1;
    }
    // 取到 hostname 的第一个ip 地址，对dest 填充
    dest.sin_addr = *(struct in_addr *)pHostent->h_addr;

    // 发起连接，三次握手
    ret = connect(sock_fd, (const struct sockaddr *)&dest, sizeof(dest));
    if (ret < 0) {
        perror("connect");
        close(sock_fd);
        return -1;
    }
    fprintf(stdout, "connection success!\n");
    return sock_fd;
}

// 定义HTTP头缓存，便于用户添加信息
typedef struct {
    char *header;
    ssize_t pos;			// 待插入的位置索引
    ssize_t capacity;
}HeaderBuffer;

HeaderBuffer *createHeaderBuffer(int size) {
    // 申请一个空间HeaderBuffer， 准备去管理 申请的请求数据包/ recv 的空间
    HeaderBuffer *buffer = (HeaderBuffer *) malloc(sizeof(HeaderBuffer));
    if (buffer == NULL) {
        fprintf(stderr, "header buffer malloc failed!\n");
        return NULL;
    }
    // 申请请求数据包的空间 request_buf， 存放 请求行， 请求体， 携带数据
    // 申请接收响应协议和数据的buf空间
    buffer->header = (char *) malloc(sizeof(char ) * size);
    memset(buffer->header, 0, sizeof(char ) * size);
    // 初始化 管理结构体的成员
    buffer->capacity = size;
    buffer->pos = 0;
    return buffer;
}

void releaseHeaderBuffer(HeaderBuffer *buffer) {
    if (buffer) {
        if (buffer->header) {
            free(buffer->header);
        }
        free(buffer);
    }
}

void add_request_start_line(HeaderBuffer *buffer, const char *method, const char *url) {
    int len;
    len = snprintf(buffer->header, buffer->capacity,
                   //GET /img/xxxxx HTTP/1.1\r\n\r\n
                   "%s %s HTTP/1.1\r\n", method, url);
    buffer->pos += len;
}

void add_request_header(HeaderBuffer *buffer, const char *key, const char *value) {
    int len;
    len = snprintf(buffer->header + buffer->pos, buffer->capacity - buffer->pos,
                   "%s: %s\r\n", key, value);
    buffer->pos += len;
}

void add_request_end_line(HeaderBuffer *buffer) {
    int len;
    len = snprintf(buffer->header + buffer->pos, buffer->capacity - buffer->pos,"\r\n");
    buffer->pos += len;
    buffer->header[buffer->pos] = 0;
}

#define MAX_REQUEST_SIZE 1024
#define MAX_RESPONSE_SIZE 1024
/**
 * 构建http 构造请求数据包
 * @param tasock_fd
 * @return
 */
HeaderBuffer *build_request_header(tasock_fdInfo *tasock_fd) {
    HeaderBuffer *header = createHeaderBuffer(MAX_REQUEST_SIZE);
    if (header == NULL) {
        return NULL;
    }
    // 构造http 请求行
    add_request_start_line(header, "GET", tasock_fd->res_path);
    // 构造 Http 的键值对
    add_request_header(header, "Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6");
    add_request_header(header, "Host", tasock_fd->hostname);
    // add_request_header(header, "Referer", "http://www.http2demo.io/");
    add_request_header(header, "User-Agent",
                       "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/114.0.0.0 "
                       "Safari/537.36 Edg/114.0.1823.37");
    // 构造 空行
    add_request_end_line(header);
    return header;
}

// send_data_bytes(sock_fd, request_header->header, request_header->pos);
ssize_t send_data_bytes(int sock_fd, const void *_data, ssize_t len) {
    // cnt 累加记录 总共发送了多少个字节数据
    ssize_t cnt = 0;
    ssize_t ret;        // 每次调用send 发送 成功发送的个数
    const char *data = _data;
    while (cnt < len) {
        // send 返回值 为成功发送的字节数
        ret = send(sock_fd, data + cnt, len - cnt, 0);
        if (ret < 0) {
            perror("send");
            break;
        }
        cnt += ret;
    }
    printf("send %ld/%ld bytes!\n", cnt, len);
    // 累计发送的字节数  和 我要发送的字节数相等
    if (cnt == len) {
        return 0;
    } else {
        return -1;
    }
}

HeaderBuffer *rcv_response_header(int sock_fd) {
    // 创建一个管理的结构体 和接收协议及数据的buf 空间
    HeaderBuffer *response = createHeaderBuffer(MAX_RESPONSE_SIZE);
    if (response == NULL) {
        return NULL;
    }
    ssize_t ret;
    response->pos = 0;
    // 第一次接收HTTP的响应
    ret = recv(sock_fd, response->header + response->pos, response->capacity - response->pos, 0);
    if (ret < 0) {
        perror("rcv");
        return response;
    }
    // 对pos 进行更新
    response->pos += ret;
    while (strstr(response->header, "\r\n\r\n") == NULL) {
        ret = recv(sock_fd, response->header + response->pos, response->capacity - response->pos, 0);
        if (ret < 0) {
            perror("rcv loop");
            break;
        }
        response->pos += ret;
        if (ret == 0) {
            printf("remote closed!\n");
            break;
        }
    }
    return response;
}

int get_status(HeaderBuffer *response) {
    // 解析响应，看是否有错误
    //HTTP/1.0 200 OK
    //|
    //tmp
    char *tmp = strstr(response->header, "HTTP/1.1 ");
    char *endpoint;
    if (tmp == NULL) {
        return -1;
    }
    //            |
    //HTTP/1.0 200 OK
    //         |
    //         tmp
    tmp += strlen("HTTP/1.1 ");

    size_t status = strtoul(tmp, &endpoint, 10);
    if (endpoint[0] != ' ') {
        fprintf(stderr, "str to unsigned long!\n");
        return -1;
    }
    if (status != 200) {
        printf("status: %ld, ready to exit!\n", status);
        return -1;
    }
    return 0;
}

// get_content_size(response_header, "Content-Length: ", &cnt_size);
// key: value\r\n
void get_content_size(HeaderBuffer *response, const char *key, size_t *len) {
    char *start, *end;
    size_t t, cnt = 0;
    start = strstr(response->header, key);
    if (start == NULL) {
        *len = 0;
        return;
    }
    end = strstr(start, "\r\n");
    if (end == NULL) {
        *len = 0;
        return;
    }
    start += strlen(key);
    while (start < end) {
        t = *start - '0';
        cnt = cnt * 10 + t;
        ++start;
    }
    *len = cnt;
}

size_t init_file(HeaderBuffer *response_header, int fd) {
    char *tmp = strstr(response_header->header, "\r\n\r\n");
    if (tmp == NULL) {
        return -1;
    }
    ssize_t ret = 0;
    if (tmp - response_header->header + 4 == response_header->pos) {
        printf("header no other data!\n");
        return 0;
    } else {
        ret = write(fd, tmp + 4, response_header->pos - (tmp - response_header->header + 4));
        if (ret < 0) {
            perror("write");
            return -1;
        }
        printf("header other data: %ld\n", ret);
        return ret;
    }
}

// receive_bytes(sock_fd, fd, cnt_size);
size_t receive_bytes(int sock_fd, int fd, size_t cnt_size) {
    char buf[2048];
    ssize_t ret;
    ssize_t cnt = 0;

    while (cnt < cnt_size) {
        ret = recv(sock_fd, buf, sizeof(buf), 0);
        if (ret <= 0) {
            perror("receive");
            break;
        }
        ret = write(fd, buf, ret);
        cnt += ret;
        printf("cnt = %ld\n", cnt);
    }
    printf("total size: %ld\n", cnt);
    return cnt;
}

// http://1153288396.rsc.cdn77.org/img/cdn77-test-563kb.jpg
//./http_client http://1153288396.rsc.cdn77.org/img/cdn77-test-563kb.jpg
int main(int argc, char *argv[]) {
    int code = 0;
    // 1. 解析argv[1]的URL地址，转换成一个任务
    if (argc != 2) {
        fprintf(stderr, "Usage: http_client URL\n");
        return -1;
    }

    // 1. 解析URL到任务体内
    tasock_fdInfo *tasock_fd = createTasock_fd(1);
    if (tasock_fd == NULL) {
        return -1;
    }
    // 根据URL  解析 主机名 资源路径  文件名 存到  这个结构体里面
    if (parser_tasock_fd(argv[1], tasock_fd) < 0) {
        code = -1;
        goto out1;
    }

    // 2. 创建socket并连接     hostname --> ip    port
    const int sock_fd = init_socket(tasock_fd);
    if (sock_fd < 0) {
        code = -1;
        goto out1;
    }

    // 3. 构造HTTP请求头，循环发送
    HeaderBuffer *request_header = build_request_header(tasock_fd);
    if (request_header == NULL) {
        code = -2;
        goto out2;
    }
    printf("ready to send!\n");
    // 发送HTTP 请求数据包
    ssize_t bytes = send_data_bytes(sock_fd, request_header->header, request_header->pos);
    if (bytes) {
        fprintf(stderr, "send data %ld bytes error!", bytes);
        releaseHeaderBuffer(request_header);
        code = -2;
        goto out2;
    }
    // 释放 申请的 管理结构体 和 存放请求数据包的buf
    releaseHeaderBuffer(request_header);

    // 4. 收取HTTP响应头，把响应头收完，有可能出错或收到完整的数据
    HeaderBuffer *response_header = rcv_response_header(sock_fd);
    if (response_header == NULL) {
        code = -2;
        goto out2;
    }
    // 函数返回，响应的协议头 已经收完了

    // 解析响应状态码
    if (get_status(response_header) < 0) {
        code = -3;
        goto out3;
    }

    // 正常的响应包
    unsigned long cnt_size = 0;
    get_content_size(response_header, "Content-Length: ", &cnt_size);
    printf("response content-length: %ld\n", cnt_size);

    // 判断是否要分离响应头和数据域
    int fd = open(tasock_fd->filename, O_CREAT | O_WRONLY | O_TRUNC, 0666);
    if (fd < 0) {
        perror("open");
        code = -3;
        goto out3;
    }
    size_t t = init_file(response_header, fd);
    cnt_size -= t;

    size_t cnt;
    cnt = receive_bytes(sock_fd, fd, cnt_size);
    if (cnt == cnt_size) {
        printf("download success! %lu\n", cnt + t);
    } else {
        printf("download failed!\n");
    }
    close(fd);
out3:
    releaseHeaderBuffer(response_header);
out2:
    close(sock_fd);
out1:
    releaseTasock_fd(tasock_fd);
    return code;
}
