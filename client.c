#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main( void ) {
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(9200);
	addr.sin_addr.s_addr = inet_addr("192.168.0.128");

	int ret = connect(fd, (struct sockaddr*)&addr, sizeof(addr));
	if ( ret == -1 )
	{
		perror("connect");
		exit(1);
	}
	
	char buf[1024] = {};
	while(fgets(buf, 1024, stdin) != NULL)
	{
		send(fd, buf, strlen(buf), 0);
		memset(buf, 0, sizeof(buf));

		int ret = recv(fd, buf, 1024, 0);
		if(ret <= 0)
		{
			break;
		}

		printf("=> : %s\n", buf);
	}
	close(fd);
}


