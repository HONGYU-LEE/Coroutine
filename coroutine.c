#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<assert.h>
#include<stdint.h>
#include"coroutine.h"

//创建协程序调度器
schedule* schedule_create()
{   
	schedule* s = (schedule*)malloc(sizeof(schedule));

	if(s == NULL)
	{   
		perror("schedule_create.\n");
		return NULL;    
	}

	s->coroutines = (coroutine**)malloc(sizeof(coroutine*) * COROUTINE_SIZE);
	memset(s->coroutines, 0x00, sizeof(coroutine*) * COROUTINE_SIZE);
	s->cur_id = -1;
	s->max_id = 0;   

	return s;
}

//协程的执行函数, 因为makecontext接收的函数参数只能为整型，所以这里要进行一层封装
static void* run_func(uint32_t low, uint32_t high)
{   
    uintptr_t ptr = (uintptr_t) low | ((uintptr_t) high << 32);
	schedule* s = (schedule*) ptr;

	if(s->cur_id != -1)
	{   
		coroutine* crt = s->coroutines[s->cur_id];

		crt->call_back(s, crt->args);
		crt->state = DEAD;
		s->cur_id = -1;
	}

	return NULL;
}

//创建协程, 并返回协程所处下标
int coroutine_create(schedule* s, void* (*call_back)(schedule*, void*), void* args)
{
	assert(s);

	coroutine* crt = NULL;
	int i = 0;
	for(i = 0; i < s->max_id; i++)
	{
		if(s->coroutines[i]->state == DEAD)
		{
			break;
		}
	}

	if(i == s->max_id || s->coroutines[i] == NULL)
	{
		s->coroutines[i] = (coroutine*)malloc(sizeof(coroutine));
		++s->max_id;
	}

	crt = s->coroutines[i];
	crt->call_back = call_back;
	crt->args = args;
	crt->state = READY;

	getcontext(&crt->ctx);

	crt->ctx.uc_stack.ss_sp = crt->stack;
	crt->ctx.uc_stack.ss_size = STACK_SIZE;
	crt->ctx.uc_flags = 0;
	crt->ctx.uc_link = &s->main;

	uintptr_t ptr = (uintptr_t) s;
	makecontext(&crt->ctx, (void (*)())&run_func, 2, (uint32_t)ptr, (uint32_t)(ptr >> 32));

	return i;
}

//获取协程状态
static enum State coroutine_state(schedule* s, int id)
{
	assert(s);
	assert(id >= 0 && id < s->max_id);

	coroutine* c = s->coroutines[id];

	if(c == NULL)
	{
		return DEAD;
	}

	return c->state;
}

//协程切换调度
void coroutine_yield(schedule* s)
{
	assert(s);

	if(s->cur_id != -1)
	{
    	coroutine* crt = s->coroutines[s->cur_id];
    	s->cur_id = -1;
	    crt->state = SUSPEND;

	    //切换调度， 此时执行流从协程切换到主流程中
        swapcontext(&crt->ctx, &s->main);
    }
}

//协程恢复调度
void coroutine_resume(schedule* s, int id)
{
	assert(s);
	assert(id >= 0 && id < s->max_id);

	coroutine* crt = s->coroutines[id];

	if(crt != NULL && crt->state == SUSPEND)
	{
		crt->state = RUNNING;
		s->cur_id = id;
		swapcontext(&s->main, &crt->ctx);
	}
}

//启动协程
void coroutine_running(schedule* s, int id)
{
	assert(s);
	assert(id >= 0 && id < s->max_id);

	if(coroutine_state(s, id) == DEAD)
	{
		return;
	}

	coroutine* crt = s->coroutines[id];
	crt->state = RUNNING;
	s->cur_id = id;

	swapcontext(&s->main, &crt->ctx);
} 

//销毁协程
static void coroutine_destroy(schedule* s, int id)
{
	coroutine* crt = s->coroutines[id];

    if(crt != NULL)
    {
        free(crt);
        s->coroutines[id] = NULL;
    }
}

//销毁协程调度器
void schedule_destroy(schedule* s)
{
    int i;

    for(i = 0; i < s->max_id; i++)
    {
        coroutine_destroy(s, i);
    }

    free(s->coroutines);
    s->coroutines = NULL;
    free(s);
    s = NULL;
}

//调度器中的协程是否全部结束，结束返回1，没结束返回0
int schedule_finished(schedule* s) 
{
	if(s->cur_id != -1)
    {
        return 0;
    }

	int i;
    
    for(i = 0; i < s->max_id; i++)
    {
        if(s->coroutines[i]->state != DEAD)
        {
            return 0;
        }
    }
	
	return 1;
}

