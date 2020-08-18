#include<ucontext.h>

#define STACK_SIZE (1024 * 1024)
#define COROUTINE_SIZE (1024)

enum State
{
    DEAD,
    RUNNING,
    READY,
    SUSPEND
};

struct schedule;

typedef struct coroutine
{
    void* (*call_back)(struct schedule* s, void* args); //回调函数
    void* args; //回调函数的参数
    ucontext_t ctx; //协程的上下文数据
    char stack[STACK_SIZE]; //协程的栈
    enum State state;   //协程状态

}coroutine;

typedef struct schedule
{
    coroutine** coroutines;    //协程数组
    int cur_id; //正在运行的协程
    int max_id; //最大的下标
    ucontext_t main; //主流程上下文数据
}schedule;

//创建协程序调度器
schedule* schedule_create();

//创建协程, 并返回协程所处下标
int coroutine_create(schedule* s, void* (*call_back)(schedule*, void* ), void* args);

//协程切换调度
void coroutine_yield(schedule* s);

//协程恢复调度
void coroutine_resume(schedule* s, int id);

//启动协程
void coroutine_running(schedule* s, int id);

//销毁携程调度器
void schedule_destroy(schedule* s);

//判断协程调度器中的协程是否全部结束, 结束返回1, 没结束返回0
int schedule_finished(schedule* s);
