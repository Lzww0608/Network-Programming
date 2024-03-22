
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <pthread.h>

#include <stdlib.h>
#include <unistd.h>

#include <sys/poll.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <arpa/inet.h>


void client_create(int id, int myPort, int peerPort) {
	//端口复用标志位
	int reuse = 1;
	
	int sockfd;
	struct sockaddr_in peer_addr;
	peer_addr.sin_family = AF_INET;
	peer_addr.sin_addr.s_addr = inet_addr("192.168.72.128");
	peer_addr.sin_port = htons(peerPort);
	
	struct sockaddr_in my_addr;
	my_addr.sin_family = AF_INET;
	//my_addr.sin_addr.s_addr = INADDR_ANY
	my_addr.sin_addr.s_addr = inet_addr("0.0.0.0");
	my_addr.sin_port = htons(myPort);
	
	//绑定UDP协议
	sockfd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (sockfd == -1) {
		perror("child socket!\n");
		exit(1);
	}
	
	//获取fd的阻塞标志
	int opt = fcntl(sockfd, F_GETFL);
	//设置fd为非阻塞模式
	fcntl(sockfd, F_SETFL, opt | O_NONBLOCK);
	
	//复用同一个端口地址 "Adrress already in use"
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse))) {
		exit(1);
	}
	//多个套接字共享一个端口
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse))) {
		exit(1);
	}
	
	
	if (bind(sockfd, (struct sockaddr*)&my_addr, sizeof(struct sockaddr))) {
		perror("child bind");
		exit(1);
	}
	
	//UDP可以不进行connect, 1.可以使用send() 2.发送错误报告 3. 过滤数据
	if (-1 == connect(sockfd, (struct sockaddr*)&peer_addr, sizeof(struct sockaddr))) {
		perror("wrong connect");
		exit(1);
	}
	
	usleep(10);
	
	char buffer[1024] = {0};
	memset(buffer, 0, 1024);
	sprintf(buffer, "hello, %d", id);
	sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr*)&peer_addr, sizeof(struct sockaddr));
	
}



void udp_serial(int num) {
	for (int i = 1; i <= num; i++) {
		client_create(i, 2024 + i, 1234);
	}
}


int main(int argc, char *argv[]) {
	udp_serial(1024);
	
	printf("udp communication success!\n");
	return 0;
}