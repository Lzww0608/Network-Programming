
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#include <stdlib.h>
#include <unistd.h>

#include <sys/poll.h>
#include <arpa/inet.h>




int local_bind(short port) {
	int connfd = socket(AF_INET, SOCK_STREAM, 0);
	
	struct sockaddr_in tcpclient_addr;
	memset(&tcpclient_addr, 0, sizeof(struct sockaddr_in));
	
	tcpclient_addr.sin_family = AF_INET;
	tcpclient_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	tcpclient_addr.sin_port = htons(port);
	
	if (-1 == bind(connfd, (struct sockaddr*)&tcpclient_addr, sizeof(struct sockaddr))) {
		perror("bind error");
		return -1;
	}
	
	return connfd;
}


int connect_tcpserver(int connfd, const char* ip, short port) {
	struct sockaddr_in tcp_server_addr;
	memset(&tcp_server_addr, 0, sizeof(struct sockaddr_in));
	
	tcp_server_addr.sin_family = AF_INET;
	inet_pton(AF_INET, ip, &tcp_server_addr.sin_addr);
	tcp_server_addr.sin_port = htons(port);
	
	int ret = connect(connfd, (struct sockaddr*)&tcp_server_addr, sizeof(struct sockaddr_in));
	
	if (ret != 0) {
		return -1;
	}
	
	return connfd;
}


void *client_thread(void* arg) {
	int clientfd = *(int *)arg;
	
	while (1) {
		
		printf("client > ");
		char buffer[128] = {0};
		scanf("%s", buffer);
		
		if (strcmp(buffer, "quit") == 0) {
			break;
		}
		
		int len = strlen(buffer);
		printf("count: %d, buffer: %s\n", len, buffer);
		
		send(clientfd, buffer, len, 0);
	}
}



int main(int argc, char *argv[]) {
	
	if (argc < 3) {
		printf("not enought argc\n");
		return -1;
	}
	
	char *ip = argv[1];
	int port = atoi(argv[2]);
	
	int sockfd = local_bind(2048);
	
	while (1) {
		int ret = connect_tcpserver(sockfd, ip, port);
		
		if (ret < 0) {
			usleep(1);
			continue;
		}
		break;
	}
	
	printf("connect success\n");
	
	pthread_t th;
	pthread_create(&th, NULL, client_thread, &sockfd);
	
	struct pollfd fds[1] = {0};
	fds[0].fd = sockfd;
	fds[0].events = POLLIN;
	
	while (1) {
		
		int ready = poll(fds, 1, -1);
		if (fds[0].revents & POLLIN) {
			char buffer[128] = {0};
			int count = recv(sockfd, buffer, 128, 0);
		
			if (count == 0) {
				fds[0].fd = -1;
				fds[0].events = 0;
				
				close(sockfd);
				break;
			}
			
			printf("recv --> count: %d, buffer: %s\n", count, buffer);
		}
		
		
	}



}