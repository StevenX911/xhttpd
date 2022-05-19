#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include "xhttpd.h"

#define MAXSIZE 2048

// 获取一行\r\n结尾的数据（此函数的目的是根据http协议内容特性而定）
// http请求和响应中，要求每行的数据都以\r\n结尾
int get_line(int cfd, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;
    while ((i < size - 1) && (c != '\n'))
    {
        n = recv(cfd, &c, 1, 0);
        if (n > 0)
        {
            if (c == '\r')
            {
                n = recv(cfd, &c, 1, MSG_PEEK);
                if ((n > 0) && (c == '\n'))
                {
                    recv(cfd, &c, 1, 0);
                }
                else
                {
                    c = '\n';
                }
            }
            buf[i] = c;
            i++;
        }
        else
        {
            c = '\n';
        }
    }
    buf[i] = '\0';
    if (-1 == n)
    {
        i = n;
    }
    return i;
}

// 获取文件类型
char *get_file_type(const char *file)
{
    char *dot;
    dot = strrchr(file, '.');
    if (dot == NULL)
    {
        return "text/plain;chartset=utf-8";
    }
    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
    {
        return "text/html;chartset=utf-8";
    }
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
    {
        return "image/jpeg";
    }
    if (strcmp(dot, ".gif") == 0)
    {
        return "image/gif";
    }
    if (strcmp(dot, ".png") == 0)
    {
        return "image/png";
    }
    if (strcmp(dot, ".css") == 0)
    {
        return "text/css;chartset=utf-8";
    }
    if (strcmp(dot, ".js") == 0)
    {
        return "application/javascript;chartset=utf-8";
    }
    if (strcmp(dot, ".mp3") == 0)
    {
        return "audio/mpeg";
    }
    if (strcmp(dot, ".mp4") == 0)
    {
        return "video/mp4";
    }
    return "text/plain;chartset=utf-8";
}

// encode
void encode_str(char *to, int tosize, const char *from)
{
    // todo
    strcpy(to, from);
}

// decode
void decode_str(char *to, char *from)
{
    // todo
    strcpy(to, from);
}

// 创建监听lfd
int init_listen_fd(int port, int epfd)
{
    // 创建监听的socket lfd
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd == -1)
    {
        perror("socket error.");
        exit(1);
    }
    // 创建服务器地址结构 ip + port
    struct sockaddr_in srv_addr;

    bzero(&srv_addr, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(port); // TODO: port valid check
    srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // 设置端口复用
    int opt = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 给lfd绑定地址结构
    int ret = bind(lfd, (struct sockaddr *)&srv_addr, sizeof(srv_addr));
    if (ret == -1)
    {
        perror("bind error.");
        exit(1);
    }

    // 设置监听的上限
    ret = listen(lfd, 128);
    if (ret == -1)
    {
        perror("listen error.");
        exit(1);
    }

    // 将lfd挂到epoll树上
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = lfd;

    ret = epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);
    if (ret == -1)
    {
        perror("epoll_ctl add lfd error.");
        exit(1);
    }

    return lfd;
}

// 接受新链接cfd
void do_accept(int lfd, int epfd)
{
    struct sockaddr_in clt_addr;
    socklen_t clt_addr_len = sizeof(clt_addr);

    int cfd = accept(lfd, (struct sockaddr *)&clt_addr, &clt_addr_len);
    if (cfd == -1)
    {
        perror("accept error.");
        exit(1);
    }

    // 打印client IP [可选]
    char clientIP[64] = {0};
    printf("new client IP:%s  PORT:%d  cfd:%d\n",
           inet_ntop(AF_INET, &clt_addr.sin_addr.s_addr, clientIP, sizeof(clientIP)),
           ntohs(clt_addr.sin_port), cfd);

    // 设置cfd非阻塞
    int flag = fcntl(cfd, F_GETFL);
    flag |= O_NONBLOCK;
    fcntl(cfd, F_SETFL, flag);

    // 将新结点cfd挂到epoll监听树
    struct epoll_event ev;
    ev.data.fd = cfd;

    // 设置边沿非阻塞触发模式
    ev.events = EPOLLIN | EPOLLET;
    int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);
    if (ret == -1)
    {
        perror("epoll_ctl add cfd error.");
        exit(1);
    }
}

// 断开链接
void disconnect(int cfd, int epfd)
{
    int ret = epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
    if (ret != 0)
    {
        perror("epoll_ctl del error");
        exit(1);
    }
    close(cfd);
}

// 发送错误页
void send_error(int cfd, int status, char *title, char *text)
{
    char buf[4096] = {0};
    sprintf(buf, "<html><head><title>%d %s</title></head><body>\r\n", status, title);
    sprintf(buf + strlen(buf), "<center><h1>%d %s</h1></center><hr>\r\n", status, title);
    if (text != NULL)
    {
        sprintf(buf + strlen(buf), "<center>%s</center>\r\n", text);
    }
    sprintf(buf + strlen(buf), "<center>xhttpd/0.0.1</center>\r\n");
    sprintf(buf + strlen(buf), "</body></html>\r\n");
    printf("error len: %lu\n", strlen(buf)); // 计算长度
    send(cfd, buf, strlen(buf), 0);
}

// 发送响应头
void send_head(int cfd, int no, char *desc, char *ctype, int len)
{
    printf("----------------%s\n", ctype);
    char buf[1024] = {0};
    sprintf(buf, "HTTP/1.1 %d %s\r\n", no, desc);
    sprintf(buf + strlen(buf), "Content-Type: %s\r\n", ctype);
    sprintf(buf + strlen(buf), "Content-Length: %d\r\n", len);
    sprintf(buf + strlen(buf), "Connection: %s\r\n", "close");
    sprintf(buf + strlen(buf), "\r\n");
    send(cfd, buf, strlen(buf), 0);
}

// 发送响应体
void send_body(int cfd, const char *file)
{
    int n = 0;
    char buf[4096] = {0};
    int fd = open(file, O_RDONLY);
    if (fd == -1)
    {
        perror("open error.");
        exit(1);
    }

    while ((n = read(fd, buf, sizeof(buf))) > 0)
    {
        // TODO: 待优化，真实网络环境下，大文件传输容易丢包
        int ret = send(cfd, buf, n, 0);
        if (ret <= 0)
        {
            if (ret == -1 && (errno == EAGAIN || errno == EINTR))
            {
                printf("-----------EAGAIN/INTER\n");
                usleep(50 * 1000); /**发送区为满时候循环等待，直到可写为止*/
                continue;
            }
            else
            {
                perror("send error");
                exit(1);
            }
        }
        else
        {
           usleep(50 * 1000); 
        }
    }
    printf("-----------------BODY n:%d\n", n);
    close(fd);
}

// 发送目录
void send_dir(int cfd, const char *dirname)
{
    int i, ret;

    char buf[4096] = {0};
    sprintf(buf, "<html><head><title>%s</title></head>", dirname);
    sprintf(buf + strlen(buf), "<body><h1>%s</h1><table>", dirname);

    char enstr[1024] = {0};
    char path[1024] = {0};

    struct dirent **ptr;
    int num = scandir(dirname, &ptr, NULL, alphasort);

    for (i = 0; i < num; i++)
    {
        char *name = ptr[i]->d_name;
        // 拼接完整路径
        sprintf(path, "%s/%s", dirname, name);
        struct stat st;
        stat(path, &st);
        // 编码
        encode_str(enstr, sizeof(enstr), name);

        if (S_ISREG(st.st_mode)) // 处理文件
        {
            sprintf(buf + strlen(buf),
                    "<tr><td><a href=%s>%s</a></td><td>%ld</td></tr>",
                    enstr, name, (long)st.st_size);
        }
        else if (S_ISDIR(st.st_mode)) // 处理目录
        {
            sprintf(buf + strlen(buf),
                    "<tr><td><a href=%s/>%s/</a></td><td>%ld</td></tr>",
                    enstr, name, (long)st.st_size);
        }

        ret = send(cfd, buf, strlen(buf), 0);

        if (ret == -1)
        {
            if (errno == EAGAIN)
            {
                perror("send error:");
                continue;
            }
            else if (errno == EINTR)
            {
                perror("send error:");
                continue;
            }
            else
            {
                perror("send error:");
                exit(1);
            }
        }
        memset(buf, 0, sizeof(buf));
    }

    sprintf(buf + strlen(buf), "</table></body></html>\r\n");
    send(cfd, buf, strlen(buf), 0);
}

// 处理http请求
void http_request(int cfd, char *path)
{
    // 解码，防止乱码
    decode_str(path, path);
    char *file = path + 1; // 获取文件名
    // 如果没有指定访问的资源，显示当前目录的内容
    if (strcmp(path, "/") == 0)
    {
        file = "./";
    }
    struct stat sbuf;
    int ret = stat(file, &sbuf);
    if (ret != 0)
    {
        printf("------------------404 Not Found.\n");
        send_head(cfd, 404, "Not Found", get_file_type(".html"), 147);
        send_error(cfd, 404, "Not Found", NULL);
        return;
    }

    if (S_ISDIR(sbuf.st_mode)) // 目录
    {
        printf("------------------is a dir.\n");
        send_head(cfd, 200, "OK", get_file_type(".html"), -1); // TODO: -1 导致连接不能关闭
        send_dir(cfd, file);
    }
    else if (S_ISREG(sbuf.st_mode)) // 普通文件
    {
        printf("------------------is a file.\n");
        // 回发http协议头
        send_head(cfd, 200, "OK", get_file_type(file), sbuf.st_size);
        // 回发http响应体
        send_body(cfd, file);
    }
}

// 解析数据
void do_read(int cfd, int epfd)
{
    // 读取一行http协议， 拆分，获取get文件及协议号
    char line[1024] = {0};
    int len = get_line(cfd, line, sizeof(line));
    if (len == 0)
    {
        printf("client closed.\n");
        disconnect(cfd, epfd);
    }
    else
    {
        char method[16], path[256], protocol[16];
        sscanf(line, "%[^ ] %[^ ] %[^ ]", method, path, protocol);
        printf("%s %s %s \n", method, path, protocol);
        while (1)
        {
            // 处理其他多余的头部，TODO：细化处理
            char buf[1024] = {0};
            len = get_line(cfd, buf, sizeof(buf));
            if (len <= 0)
            {
                break;
            }
        }

        if (strncasecmp(method, "GET", 3) == 0)
        {
            http_request(cfd, path); // 处理http请求
        }
        if (strncasecmp(method, "POST", 4) == 0)
        {
            // TODO
        }
    }
}

// 创建epoll
void epoll_run(int port)
{
    int i = 0;
    struct epoll_event all_events[MAXSIZE];
    // 创建一个epoll监听树根【红黑树】
    int epfd = epoll_create(MAXSIZE);
    if (epfd == -1)
    {
        perror("epoll_create error.");
        exit(1);
    }

    // 创建监听lfd，并添加到epoll监听树
    int lfd = init_listen_fd(port, epfd);
    while (1)
    {
        // 监听结点对应的事件 <阻塞>
        int ret = epoll_wait(epfd, all_events, MAXSIZE, -1);
        if (ret == -1)
        {
            if (errno == EAGAIN || errno == EINTR)
            {
                continue;
            }
            else
            {
                perror("epoll_wait error.");
                exit(1);
            }
        }

        for (i = 0; i < ret; i++)
        {
            // 仅处理读事件， 其他事件默认不处理
            struct epoll_event *pev = &all_events[i];
            // 不是读事件,跳过
            if (!(pev->events & EPOLLIN))
            {
                continue;
            }
            if (pev->data.fd == lfd) // 接受链接请求
            {
                do_accept(lfd, epfd);
            }
            else
            {
                do_read(pev->data.fd, epfd);
            }
        }
    }
}

int main(int argc, char const *argv[])
{
    // 通过命令行参数获取启动的端口号和server目录
    if (argc < 3)
    {
        printf("./xhttpd port dir\n");
        exit(1);
    }
    // 获取端口
    int port = atoi(argv[1]);
    // 改变当前进程工作目录
    int ret = chdir(argv[2]);
    if (ret != 0)
    {
        perror("chdir error.");
        exit(1);
    }
    // 启动epoll监听
    epoll_run(port);
    return 0;
}