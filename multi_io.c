//multi_io.c
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/poll.h>
#include <sys/epoll.h>

void *client_thread(void *arg) {
    int clientfd = *(int*) arg;
    while (1) {
        char buffer[128] = {0};
        int count = recv(clientfd, buffer, 128, 0);
        if (count == 0) { //调用close()
            break;
       }
        send(clientfd, buffer, count/*128*/, 0);
        printf("clientfd: %d, count: %d, buffer: %s\n", clientfd, count, buffer);
    }
    close(clientfd);
}

//tcp
int main() {
    //stdin: 0 stdout: 1 stderr: 2
	int sockfd = socket(AF_INET, SOCK_STREAM, 0); //3
    
    struct sockaddr_in serveraddr;
    memset(&serveraddr, 0, sizeof(struct sockaddr_in));
    
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(2048);
    //排班
    if (-1 == bind(sockfd, (struct sockaddr*)&serveraddr, sizeof(struct sockaddr))) {
        perror("bind");
        return -1;
    }
    	//4
    listen(sockfd, 10); //开始工作
#if 0
    struct sockaddr_in clientaddr;
	socklen_t len = sizeof(clientaddr);
    int clientfd = accept(sockfd, (struct sockaddr*)&clientaddr, &len); // 4
	printf("accept\n");
#if 0
    char buffer[128] = {0};
    int count = recv(clientfd, buffer, 128, 0);
    send(clientfd, buffer, count/*128*/, 0);
#else
    
    while(1) {
        char buffer[128] = {0};
        int count = recv(clientfd, buffer, 128, 0);
        if (count == 0) { //调用close()
            break;
       }
        send(clientfd, buffer, count/*128*/, 0);
        printf("sockfd: %d, clientfd: %d, count: %d, buffer: %s\n", sockfd, clientfd, count, buffer);
    }
#endif
#elif 0
    while(1) {
        struct sockaddr_in clientaddr;
        socklen_t len = sizeof(clientaddr);
        int clientfd = accept(sockfd, (struct sockaddr*)&clientaddr, &len); // 4
        pthread_t thid;
        pthread_create(&thid, NULL, client_thread, &clientfd);
       }
#elif 0 //select 单线程处理多个客户端
	//0:stdin 1:stdout 2:stderr 3:listen 
	//int nready = select(maxfd, rset, wset, eset, timeout); //判断最大fd的值, 可读, 可写, 出错, 轮循间隔
	/*typedef struct {
		unsigned long fds_bits[1024 / (8 * sizeof(long))];
	} __kernel_fd_set;*/
	
	fd_set rfds, rset;
	FD_ZERO(&rfds);
	FD_SET(sockfd, &rfds);
	
	int maxfd = sockfd;
	
	printf("loop\n");
	
	while (1) {
		rset = rfds;
		
		int nready = select(maxfd + 1, &rset, NULL, NULL, NULL/*一直等待*/);
		
		if (FD_ISSET(sockfd, &rset)) {
			struct sockaddr_in clientaddr;
			socklen_t len = sizeof(clientaddr);
			
			int clientfd = accept(sockfd, (struct sockaddr*)&clientaddr, &len);
			
			printf("sockfd: %d\n", sockfd);
			
			FD_SET(clientfd, &rfds);
			
			maxfd = clientfd;
		}
		int i = 0;
		for (i = sockfd + 1; i <= maxfd; i++) {
			if (FD_ISSET(i, &rset)) {
				char buffer[128] = {0};
				int count = recv(i, buffer, 128, 0);
				if (count == 0) { //调用close()
					printf("disconnect\n");
					//close(i); ①
					//FD_CLR(i, &rfds); ②
					//标准断开：清空多路io复用再关闭 ③
					FD_CLR(i, &rfds);
					close(i);
					
					break;
			   }
				send(i, buffer, count/*128*/, 0);
				printf("sockfd: %d, clientfd: %d, count: %d, buffer: %s\n", sockfd, i, count, buffer);
			}
		}
	}
	
#elif 0
	//poll
/*
struct pollfd {
	int fd;
	short events; //传入
	short revents; //返回
}
*/
	struct pollfd fds[1024] = {0};
	
	fds[sockfd].fd = sockfd;
	fds[sockfd].events = POLLIN;
	
	int maxfd = sockfd;
	
	while (1) {
		
		int nready = poll(fds, maxfd + 1, -1/*timeout*/);
		
		if (fds[sockfd].revents && POLLIN) {
			
			struct sockaddr_in clientaddr;
			socklen_t len = sizeof(clientaddr);
			
			int clientfd = accept(sockfd, (struct sockaddr*)&clientaddr, &len);
			
			printf("sockfd: %d\n", sockfd);
			
			fds[sockfd].fd = clientfd;
			fds[sockfd].events = POLLIN;
			
			maxfd = clientfd;
		}
		
		int i = 0;
		for(i = sockfd + 1; i <= maxfd; i++) {
			if (fds[i].revents && POLLIN) {
				char buffer[128] = {0};
				int count = recv(i, buffer, 128, 0);
				if (count == 0) { //调用close()
					printf("disconnect\n");
					fds[i].fd = -1;
					fds[i].events = 0;
					
					close(i);
					continue;
			   }
				send(i, buffer, count/*128*/, 0);
				printf("sockfd: %d, clientfd: %d, count: %d, buffer: %s\n", sockfd, i, count, buffer);
			}
		}
	}
	
	
#else 
	//epoll
	int epfd = epoll_create(1/*大于0即可，遗留参数*/); //int size
	
	struct epoll_event ev;//成员: events, data
	ev.events = EPOLLIN;
	ev.data.fd = sockfd;
	
	epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev);
	
	struct epoll_event events[1024] = {0};
	while (1) {
		int nready = epoll_wait(epfd, events, 1024, -1);
		
		int i = 0;
		for (i = 0; i < nready; i++) {
			
			int connfd = events[i].data.fd;
			if (sockfd == connfd) {
				struct sockaddr_in clientaddr;
				socklen_t len = sizeof(clientaddr);
				
				int clientfd = accept(sockfd, (struct sockaddr*)&clientaddr, &len);
				
				printf("clientfd: %d\n", clientfd);
				//ev.events = EPOLLIN;
				ev.events = EPOLLIN | EPOLLET;
				ev.data.fd = clientfd;
				epoll_ctl(epfd, EPOLL_CTL_ADD, clientfd, &ev);
			} else if (events[i].events && EPOLLIN) {
				
				char buffer[128] = {0};
				int count = recv(connfd, buffer, 5, 0);
				if (count == 0) { //调用close()
					printf("disconnect\n");
					
					epoll_ctl(epfd, EPOLL_CTL_DEL, connfd, NULL);
					close(i);
					continue;
			   }
				send(connfd, buffer, count/*128*/, 0);
				printf("sockfd: %d, clientfd: %d, count: %d, buffer: %s\n", sockfd, connfd, count, buffer);
			}
		}
	}
	
#endif
	
    //close(clientfd);
    
    getchar();
}