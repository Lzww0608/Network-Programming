//基于epoll，面向事件的IO，进行事件分离

#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <pthread.h>
#include <sys/poll.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define BUFFER_LENGTH 512

#define ENABLE_HTTP_REPONSE 1024


#if ENABLE_HTTP_REPONSE

typedef int (*RCALLBACK)(int fd); 

//处理事件结构
struct conn_item {
	int fd;
	//读缓冲区
	char rbuffer[BUFFER_LENGTH];
	int rlen;
	//写缓冲区
	char wbuffer[BUFFER_LENGTH];
	int wlen;  
	
	char resource[BUFFER_LENGTH];// abc.html
	
	union {
		RCALLBACK accept_callback;
		RCALLBACK recv_callback;
	} recv_t;
	RCALLBACK send_callback;
};

int epfd = 0;
//处理事件列表（其实这么写有些不专业，开的太大了...）
struct conn_item connlist[1024576] = {0};
//Linux时间戳
struct timeval tv_start;

#define TIME_SUB_MS(tv1, tv2) ((tv1.tv_sec - tv2.tv_sec) * 1000 + (tv1.tv_usec - tv2.tv_usec) / 1000)

typedef struct conn_item connection_t;

int http_response(connection_t *conn) {
	
	conn->wlen = sprintf(conn->wbuffer, 
		"HTTP/1.1 200 OK\r\n"
		"Accept-Ranges: bytes\r\n"
		"Content-Length: 82\r\n"
		"Content-Type: text/html\r\n"
		"Date: Sat, 06 Aug 2023 13:16:46 GMT\r\n\r\n"
		"<html><head><title>0voice.lzww</title></head><body><h1>lzww</h1></body></html>\r\n\r\n");
	
	return conn->wlen;
}

int http_request(connection_t *conn) {
	return 0;
}

#endif





int accept_cb(int fd);
int recv_cb(int fd);
int send_cb(int fd);




#if 0
struct reactor {
	int efpd;
	struct conn_item *connlist;
};

#else
	
#endif

/*
struct epoll_event {
	__poll_t events; // EPOLLIN, EPOLLOUT
	__u64 data;
} EPOLL_PACKED;

union {
    void *ptr;
    int fd;
    __uint32_t u32;
    __uint64_t u64;
} data;

*/
//事件分离器
int set_event(int fd, int event, int flag/*标志位*/) {
	//接收到事件后，调用epoll_ctl()之后再交给事件处理器
	if (flag == 1) { //1 add，0 modify
		struct epoll_event ev;
		ev.events = event;
		ev.data.fd = fd;
		epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
	} else {
		struct epoll_event ev;
		ev.events = event;
		ev.data.fd = fd;
		epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
	}
	
}

int accept_cb(int fd) {
	struct sockaddr_in clientaddr;
	socklen_t len = sizeof(clientaddr);
	
	int clientfd = accept(fd, (struct sockaddr*)&clientaddr, &len);
	if (clientfd < 0) {
		return -1;
	}
	
	
	
	set_event(clientfd, EPOLLIN, 1);
	
	connlist[clientfd].fd = clientfd;
	memset(connlist[clientfd].rbuffer, 0, BUFFER_LENGTH);
	memset(connlist[clientfd].wbuffer, 0, BUFFER_LENGTH);
	connlist[clientfd].rlen = 0;
	connlist[clientfd].wlen = 0;
	
	connlist[clientfd].recv_t.recv_callback = recv_cb;
	connlist[clientfd].send_callback = send_cb;
	//统计每一千个连接用时
	if ((clientfd % 1000) == 999) {
		struct timeval tv_cur;
		gettimeofday(&tv_cur, NULL);
		int time_used = TIME_SUB_MS(tv_cur, tv_start);
		
		memcpy(&tv_start, &tv_cur, sizeof(struct timeval));
		
		printf("clientfd: %d, time_used: %d\n", clientfd, time_used);
	}
	
	return clientfd;
}

//recv
//buffer --> 
int recv_cb(int fd) { //fd --> EPOLLIN
	char *buffer = connlist[fd].rbuffer;
	int idx = connlist[fd].rlen;
	
	int count = recv(fd, buffer + idx, BUFFER_LENGTH - idx, 0);
	if (count == 0) {
		//printf("disconnect\n");
		
		epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
		close(fd);
		
		return -1;
	}
	 
	connlist[fd].rlen += count;
	
#if 1 //echo: need to send
	memcpy(connlist[fd].wbuffer, connlist[fd].rbuffer, connlist[fd].rlen);
	connlist[fd].wlen = connlist[fd].rlen;
	connlist[fd].rlen -= connlist[fd].rlen;
	
#else 
	//http_request
	http_request(&connlist[fd]);
	http_response(&connlist[fd]);

#endif 
	//修改event
	set_event(fd, EPOLLOUT, 0);
	
	return count;
}


int send_cb(int fd) {
	char *buffer = connlist[fd].wbuffer;
	int idx = connlist[fd].wlen;
	
	int count = send(fd, buffer, idx, 0);
	
	set_event(fd, EPOLLIN, 0);

	
	return count;
}

//端口开启、绑定并开始监听，同multi_io.c
int init_server(unsigned short port) {
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	
	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(struct sockaddr_in));
	
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(port);
	
	if (-1 == bind(sockfd, (struct sockaddr*)&serveraddr, sizeof(struct sockaddr))) {
		perror("bind");
		return -1;
	}
		
	listen(sockfd, 10);
	
	return sockfd;
}


int main() {
	//epoll驱动
	epfd = epoll_create(1); // int size
	
	//同时监听IO端口上限
	int port_count = 20;
	unsigned short port = 2048;
	int i = 0;
	//轮循
	for (i = 0; i < port_count; i++) {
		//初始化服务器端口
		int sockfd = init_server(port + i);
		//绑定监听端口并开始监听
		connlist[sockfd].fd = sockfd;
		connlist[sockfd].recv_t.accept_callback = accept_cb;
		set_event(sockfd, EPOLLIN, 1);
	}
	
	gettimeofday(&tv_start, NULL);

	
	
	

		
	struct epoll_event events[1024] = {0};
		
	//main loop
	while (1) {
		int nready = epoll_wait(epfd, events, 1024, -1);
		//回调函数进行时间处理
		int i = 0;
		for (i = 0; i < nready; i++) {
			int connfd = events[i].data.fd;
			
			if (events[i].events & EPOLLIN) {
				
				int count = connlist[connfd].recv_t.recv_callback(connfd);
				//printf("recv count: %d <-- buffer: %s\n", count, connlist[connfd].rbuffer);
				
			} else if (events[i].events & EPOLLOUT) {
				int count = connlist[connfd].send_callback(connfd);
				//printf("send --> buffer: %s\n", connlist[connfd].wbuffer);
			}
		}
	}
	
	getchar();
}