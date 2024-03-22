
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <pthread.h>
#include <assert.h>

#define SO_REUSEPORT    15

#define MAXBUFFER 1024

#define PORT 1234 	//服务器端口号
#define MAXEPOLLSIZE 100

int cnt = 0;

int read_data(int fd) {
	char buffer[MAXBUFFER + 1];
	
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);
	
	memset(buffer, 0, MAXBUFFER + 1);
	
	int ret = 0;
	if ((ret = recvfrom(fd, buffer, MAXBUFFER, 0, (struct sockaddr*)&client_addr, &client_len)) > 0) {
		printf("read[%d]: %s from %d\n", ret, buffer, fd);
	} else {
		printf("read error: %s  %d\n", strerror(errno), ret);
	}
}


int udp_accpet(int fd, struct sockaddr_in my_addr) {
	int new_fd = -1, ret = 0, reuse = 1;
	char buffer[16] = {0};
	struct sockaddr_in peer_addr;
	socklen_t peer_len = sizeof(peer_addr);
	
	if (recvfrom(fd, buffer, 16, 0, (struct sockaddr*)&peer_addr, &peer_len) < 0) {
		return -1;
	}
	
	if ((new_fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		perror("child socket\n");
		exit(1);
	} else {
		printf("%d, parent: %d, new: %d\n", cnt++, fd, new_fd);
	}
	//复用同一个端口地址
	if (setsockopt(new_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse))) {
		exit(1);
	}
	//多个套接字共享同一个端口
	if (setsockopt(new_fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse))) {
		exit(1);
	}
	
	
	if (bind(new_fd, (struct sockaddr*)&my_addr, sizeof(struct sockaddr))) {
		perror("child bind!\n");
		exit(1);
	}
	
	//可以不connect
	peer_addr.sin_family = AF_INET;
	if (connect(new_fd, (struct sockaddr *)&peer_addr, sizeof(struct sockaddr)) == -1) {
		perror("child connect!\n");
		exit(1);
	}

	return new_fd;
}



int main(int argc, char* argv[]) {
	int listener, udpfd, nfds;
	socklen_t len;
	
	struct sockaddr_in my_addr, peer_addr;
	unsigned int port = PORT;
	struct epoll_event ev;
	struct epoll_event events[MAXEPOLLSIZE];
	
	int reuse = 1;
	
	if (-1 == (listener = socket(AF_INET, SOCK_DGRAM, 0))) {
		perror("socket wrong!\n");
		exit(1);
	} else {
		printf("socket success!\n");
	}
	
	if(setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse))) {
		exit(1);
	}
	
	if (setsockopt(listener, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse))) {
		exit(1);
	}
	
	int opt = fcntl(listener, F_GETFL, 0);
	fcntl(listener, F_SETFL, opt | O_NONBLOCK);
	
	memset(&my_addr, 0, sizeof(my_addr));
	my_addr.sin_family = AF_INET;
	my_addr.sin_addr.s_addr = INADDR_ANY;
	my_addr.sin_port = htons(port);
	
	if (-1 == bind(listener, (struct sockaddr*)&my_addr, sizeof(struct sockaddr))) {
		perror("bind error!\n");
		exit(1);
	} else {
		printf("IP bind success!\n");
	}
	
	udpfd = epoll_create(MAXEPOLLSIZE);
	
	//边缘触发
	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = listener;
	
	if (epoll_ctl(udpfd, EPOLL_CTL_ADD, listener, &ev) < 0) {
		fprintf(stderr, "epoll set insertion error: fd = %d\n",listener);
		return -1;
	} else {
		printf("ep add OK\n");
	}
	
	while (1) {
		
		nfds = epoll_wait(udpfd, events, 10000, -1);
		if (nfds == -1) {
			perror("epoll_wait");
			break;
		}
		
		int i = 0;
		for (i = 0; i < nfds; i++) {
			if (events[i].data.fd == listener) {
				
				int new_fd;
				struct epoll_event child_ev;
				
				while (1) {
					new_fd = udp_accpet(listener, my_addr);
					if (new_fd == -1)
						break;
					
					child_ev.events = EPOLLIN;
					child_ev.data.fd = new_fd;
					if (epoll_ctl(udpfd, EPOLL_CTL_ADD, new_fd, &child_ev) < 0) {
						fprintf(stderr, "epoll set insertion error: fd = %d\n", new_fd);
						return -1;
					}
				}
			} else {
				read_data(events[i].data.fd);
			}
		}
	}
	
	close(listener);
	return 0;
}


