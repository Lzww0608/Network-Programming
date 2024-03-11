#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>


void *client_thread(void* arg) {
	int clientfd = *(int*) arg;
	while (1) {
		char buffer[128] = {0};
		int count = recv(clientfd, buffer, 128, 0);
		if (count == 0) {
			break;
		}
		send(clientfd, buffer, count, 0);
		printf("clientfd: %d, count: %d, buffer: %s\n", clientfd, count, buffer);
		//需要添加"\n"刷新缓冲区以输出
		
	}
	close(clientfd);
}

int main() {
	//创建一个新的套接字
	
	int sockfd = socket(AF_INET/*地址类型：IPv4*/, SOCK_STREAM/*流式，表示基于TCP*/, 0/*默认协议*/);
	//struct sockaddr_in 用于存储互联网地址，指定目标地址和绑定端口
	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(struct sockaddr_in));
	//sin_family：这个成员用于指定地址族。对于 IPv4 地址，它应该被设置为 AF_INET。
	serveraddr.sin_family = AF_INET;
	//这个表达式引用 serveraddr 这个 sockaddr_in 结构体中的 sin_addr 字段的 s_addr 成员。它用于设置或检索服务器的 IP 地址。
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	//sin_port：这个成员表示端口号。它用于标识网络服务的特定端口。端口号需要使用网络字节顺序，因此在设置它时通常会使用 htons() 函数（"host to network short"）来转换端口号。
	serveraddr.sin_port = htons(4096);
	
	//将套接字绑定到网络地址上，并指定大小
	if (-1 == bind(sockfd, (struct sockaddr*)&serveraddr, sizeof(struct sockaddr))){
		perror("bind");
		return -1;
	}
	
	listen(sockfd, 10);//开始作为服务端接受连接与信息

#if 0
	struct sockaddr_in clientaddr;
	socklen_t len = sizeof(clientaddr);
	int clientfd = accept(sockfd, (struct sockaddr*)&clientaddr, &len);
	printf("accept\n");
	
#if 0
	//指定缓冲区
	char buffer[128] = {0};
	//缓冲区接收流的长度
	int count = recv(clientfd, buffer, 128, 0);
	//发送接收到的内容
	send(clientfd, buffer, count, 0);
	
#else 
	while (1) {
		char buffer[128] = {0};
		//缓冲区接收流的长度
		int count = recv(clientfd, buffer, 128, 0);
		if (count == 0) {
			break;
		}
		send(clientfd, buffer, count, 0);
		printf("sockfd: %d, clientfd: %d, count: %, buffer: %s", sockfd, clientfd, count, buffer);
		
	}
#endif

#else 
	while(1) {
		struct sockaddr_in clientaddr;
		socklen_t len = sizeof(clientaddr);
		int clientfd = accept(sockfd, (struct sockaddr*)&clientaddr, &len);
		//建立线程，一个网络连接对应一个线程
		pthread_t thid;//线程标识
		pthread_create(&thid, NULL, client_thread, &clientfd);
	}
#endif
	
	getchar();

}
