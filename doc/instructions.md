[toc]

# 使用介绍

## 上下文环境
对于上下文环境的切换可以使用很多方法
- 汇编
- C语言库函数setjmp， longjmp
- glibc的ucontext组

我在这里使用的是ucontext组件，如果不了解这个组件的使用方法，可以参考我的另一篇博客
[ucontext族函数的使用及原理分析](https://blog.csdn.net/qq_35423154/article/details/108064083)

由于ucontext只支持posix，如果需要移植到windows，只需要将api换成fiber的即可

--------
## 宏
```c
#define STACK_SIZE (1024 * 1024)	//协程函数栈大小
#define COROUTINE_SIZE (1024)		//协程最大数量
```

-----------
## 协程状态
协程共设置了四种状态，READY, RUNNING, DEAD, SUSPEND。下图描述了所有状态即状态之间的转换关系。
![在这里插入图片描述](https://img-blog.csdnimg.cn/20200819110744654.jpg?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzM1NDIzMTU0,size_16,color_FFFFFF,t_70#pic_center)

----------
## 协程与调度器结构体
```c
typedef struct coroutine
{
    void* (*call_back)(struct schedule* s, void* args); //回调函数
    void* args; //回调函数的参数
    ucontext_t ctx; //协程的上下文
    char stack[STACK_SIZE]; //协程的函数栈
    enum State state;   //协程状态

}coroutine;

typedef struct schedule
{
    coroutine** coroutines; //协程数组
    int cur_id; //正在运行的协程下标
    int max_id; //数组中最大的下标
    ucontext_t main; //主流程上下文
}schedule;
```

----------
## 接口

```c
//创建协程序调度器
schedule* schedule_create();

//创建协程, 并返回协程所处下标
int coroutine_create(schedule* s, void* (*call_back)(schedule*, void* ), void* args);

//协程让出CPU，返回主流程上下文
void coroutine_yield(schedule* s);

//恢复协程上下文
void coroutine_resume(schedule* s, int id);

//运行协程
void coroutine_running(schedule* s, int id);

//销毁协程调度器
void schedule_destroy(schedule* s);

//判断协程调度器中的协程是否全部结束, 结束返回1, 没结束返回0
int schedule_finished(schedule* s);
```



-----------
# 示范用例
## 使用协程实现一个TCP服务器
思路图
![在这里插入图片描述](https://img-blog.csdnimg.cn/20200819110530293.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzM1NDIzMTU0,size_16,color_FFFFFF,t_70#pic_center)



```c
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

int socket_init()
{
	int lst_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(lst_fd == -1)
	{
		perror("socket.\n");
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
```
可以看到该服务器可以快速的响应多个连接
![在这里插入图片描述](https://img-blog.csdnimg.cn/20200818182337114.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzM1NDIzMTU0,size_16,color_FFFFFF,t_70#pic_center)