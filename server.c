
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>

#include "coroutine.h"
static int co_ids[COROUTINE_SIZE];

void SetNoBlock(int fd) 
{
	int flag = fcntl(fd, F_GETFL, 0);
	
	flag |= O_NONBLOCK;
	fcntl(fd, F_SETFL, flag);
}
/*
int socket_init()
{
	int lst_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(lst_fd == -1)
	{
		perror("socket.");
		exit(1);
	}
	
	int op = 1;
	setsockopt(lst_fd, SOL_SOCKET, SO_REUSEADDR, &op, sizeof(op));

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(9200);
	addr.sin_addr.s_addr = inet_addr("192.168.0.128");

	if(bind(lst_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
	{
		perror("bind.\n");
		exit(1);
	}
	
	if(listen(lst_fd, SOMAXCONN) < 0)
	{	
		perror("listen.\n");
		exit(1);
	}
	return lst_fd;
}
*/
void accept_conn(int lst_fd, schedule *s, int* co_ids, void *(*call_back)(schedule *s, void *args)) 
{
	while(1)
	{
		int new_fd = accept(lst_fd, NULL, NULL);
		//如果有新连接到来则创建一个协程来管理这个连接
		if(new_fd > 0)
		{
			SetNoBlock(new_fd);

			int args[] = {lst_fd, new_fd};
			int cid = coroutine_create(s, call_back, args);
			if(cid >= COROUTINE_SIZE)
			{
				perror("too many connections.\n");
				return;
			}

			co_ids[cid] = 1;
			/*
			int i;
			for(i = 0; i < COROUTINE_SIZE; i++)
			{
				if(co_ids[i] == -1)
				{
					co_ids[i] = cid;
					break;
				}
			}
			*/
			coroutine_running(s, cid);
		}
		//如果当前没有连接，则切换至协程上下文中继续运行
		else
		{
			int i = 0;
			for(i = 0; i < COROUTINE_SIZE; i++)
			{
				if(co_ids[i] == -1)
				{
					continue;
				}
				coroutine_resume(s, i);
			}
		}
	}

}

void *handle(schedule  *s, void *args)
{
	int* arr = (int*)args;
	int cfd = arr[1];

	char buf[1024] = { 0 };
	while(1)
	{
		memset(buf, 0, sizeof(buf));

		int ret = recv(cfd, buf, 1024, 0);
		
		if(ret < 0)
		{
			//如果此时没有数据，则不再等待，直接切换回主流程
			coroutine_yield(s);
		}
		else if(ret == 0)
		{
			//通信结束
			co_ids[s->cur_id] = -1;
			break;
		}
		else
		{
			printf("=> : %s\n", buf);
			if(strncasecmp(buf, "exit", 4) == 0)
			{
				co_ids[s->cur_id] = -1;
				break;
			}

			send(cfd, buf, ret, 0);
		}
	}
}
int socket_init() {
	int lfd = socket(AF_INET, SOCK_STREAM, 0);
	if ( lfd == -1 ) perror("socket"),exit(1);

	int op = 1;
	setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &op, sizeof(op));
	
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(9200);
	addr.sin_addr.s_addr = inet_addr("192.168.0.128");
	int r = bind(lfd, (struct sockaddr*)&addr, sizeof(addr));
	if ( r == -1 ) perror("bind"),exit(1);
	
	listen(lfd, SOMAXCONN);

	return lfd;
}


int main()
{
	static int co_ids[COROUTINE_SIZE];
	int lst_fd = socket_init();
	SetNoBlock(lst_fd);

	schedule* s = schedule_create();
	
	int i;
	for(i = 0; i < COROUTINE_SIZE; i++)
	{
		co_ids[i] = -1;
	}

	accept_conn(lst_fd, s, co_ids, handle);
	schedule_destroy(s);
}








