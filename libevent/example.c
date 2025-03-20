
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <assert.h>


#include <event2/event.h>
#include <event2/bufferevent.h>

#define MAX_LINE 1024
#define LISTEN_PORT 9999
#define LISTEN_BACKLOG 1024 // backlog 连接队列大小


void do_accept(evutil_socket_t listener, short event, void *arg);
void read_cb(struct bufferevent *bev, void *arg);
void error_cb(struct bufferevent *bev, short event, void *arg);
void write_cb(struct bufferevent *bev, void *arg);

int main(int argc, char *argv[]) {
    int ret;                                      // 用于存储函数返回值
    evutil_socket_t listener;                     // 声明监听套接字
    listener = socket(AF_INET, SOCK_STREAM, 0);   // 创建TCP套接字
    assert(listener > 0);                         // 确保套接字创建成功
    evutil_make_listen_socket_reuseable(listener); // 设置套接字为可重用，避免bind时"地址已在使用"错误

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = 0;
    sin.sin_port = htons(LISTEN_PORT);

    if (bind(listener, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        perror("bind");
        return 1;
    }

    if (listen(listener, LISTEN_BACKLOG) < 0) {
        perror("listen");
        return 1;
    }

    printf("Server started on port %d\n", LISTEN_PORT);

    evutil_make_socket_nonblocking(listener);

    struct event_base *base = event_base_new();   // 创建event_base，这是libevent的核心结构
    assert(base != NULL);                         // 确保event_base创建成功
    struct event *listen_event;                   // 声明事件指针
    listen_event = event_new(base, listener, EV_READ | EV_PERSIST, do_accept, (void*)base);  // 创建监听事件，EV_READ表示可读事件，EV_PERSIST表示持久化事件
    event_add(listen_event, NULL);                // 将事件添加到event_base中，NULL表示无超时
    event_base_dispatch(base);                         // 启动事件循环，程序将在此阻塞直到所有事件处理完毕

    printf("Ends\n");
    event_base_free(base);
    return 0;
}

void do_accept(evutil_socket_t listener, short event, void *arg) {
    struct event_base *base = (struct event_base *)arg;
    evutil_socket_t fd;
    struct sockaddr_in sin;
    socklen_t slen = sizeof(sin);
    fd = accept(listener, (struct sockaddr *)&sin, &slen);
    if (fd < 0) {
        perror("accept");
        return;
    }
    if (fd > FD_SETSIZE) {
        perror("fd > FD_SETSIZE\n");
        return;
    }

    printf("Accept new connection from %s:%d\n", inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));

    struct bufferevent *bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);  // 创建bufferevent，BEV_OPT_CLOSE_ON_FREE表示bufferevent释放时关闭底层套接字
    bufferevent_setcb(bev, read_cb, NULL, error_cb, arg);  // 设置回调函数：read_cb处理读事件，无写事件回调，error_cb处理错误事件
    bufferevent_enable(bev, EV_READ | EV_WRITE | EV_PERSIST);  // 启用读写事件，EV_PERSIST使事件持久化（触发后不会被删除）
}

void read_cb(struct bufferevent *bev, void *arg) {
    char line[MAX_LINE + 1];
    int n;
    evutil_socket_t fd = bufferevent_getfd(bev);

    while (n = bufferevent_read(bev, line, MAX_LINE), n > 0) {
        line[n] = '\0';
        printf("fd=%u, read line: %s\n", fd, line);

        bufferevent_write(bev, line, n);
    } 
}

void write_cb(struct bufferevent *bev, void *arg) {
    
}

void error_cb(struct bufferevent *bev, short event, void *arg) {
    evutil_socket_t fd = bufferevent_getfd(bev);
    printf("fd = %u, ", fd);
    if (event & BEV_EVENT_EOF) {
        printf("connection closed\n");
    } else if (event & BEV_EVENT_ERROR) {
        printf("some other error\n");
    }
    bufferevent_free(bev);
}