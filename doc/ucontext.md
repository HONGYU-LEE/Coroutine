[toc]

-----------

# ucontext介绍
## 寄存器介绍
![在这里插入图片描述](https://img-blog.csdnimg.cn/20200817204753303.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzM1NDIzMTU0,size_16,color_FFFFFF,t_70#pic_center)
以上是ucontext使用到的所有寄存器，下面对他们做一些简单的介绍。
- %rax作为函数返回值使用
- %rsp栈指针寄存器， 指向栈顶
- %rdi, %rsi, %rdx, %rcx, %r8, %r9用作函数的参数，从前往后依次对应第1、第2、.....第n参数
- %rbx, %rbp, %r12, %r13, %r14, %r15用作数据存储，遵循被调用这使用规则，调用子函数之前需要先备份，防止被修改。
- %r10, %r11用作数据存储，遵循调用者使用规则，使用前需要保存原值
-------
## ucontext_t结构体

```c
typedef struct ucontext
  {
    unsigned long int uc_flags;
    struct ucontext *uc_link;// 当前上下文执行完了，恢复运行的上下文 
    stack_t uc_stack;// 该上下文中使用的栈
    mcontext_t uc_mcontext;// 保存当前上下文，即各个寄存器的状态
    __sigset_t uc_sigmask;// 保存当前线程的信号屏蔽掩码
    struct _libc_fpstate __fpregs_mem;
  } ucontext_t;
```

```c
//描述整个上下文
typedef struct
  {
    gregset_t gregs;//用于装载寄存器
    /* Note that fpregs is a pointer.  */
    fpregset_t fpregs;//所有寄存器的类型
    __extension__ unsigned long long __reserved1 [8];
} mcontext_t;

-----------------------------
//所包含的具体上下文信息
struct _libc_fpxreg
{ 
  unsigned short int significand[4];
  unsigned short int exponent;
  unsigned short int padding[3];
};

struct _libc_xmmreg
{ 
  __uint32_t    element[4];
};

struct _libc_fpstate
{ 
  /* 64-bit FXSAVE format.  */
  __uint16_t        cwd;
  __uint16_t        swd;
  __uint16_t        ftw;
  __uint16_t        fop;
  __uint64_t        rip;
  __uint64_t        rdp;
  __uint32_t        mxcsr;
  __uint32_t        mxcr_mask;
  struct _libc_fpxreg   _st[8];
  struct _libc_xmmreg   _xmm[16];
  __uint32_t        padding[24];
};

----------------------
//装载所有寄存器的容器
__extension__ typedef long long int greg_t;

/* Number of general registers.  */
#define NGREG   23

/* Container for all general registers.  */
typedef greg_t gregset_t[NGREG];

```

--------
## getcontext
```c
int getcontext(ucontext_t *ucp);
```
将当前的寄存器信息保存到变量ucp中。       

下面看看汇编代码

```c
/*  int __getcontext (ucontext_t *ucp)

  Saves the machine context in UCP such that when it is activated,
  it appears as if __getcontext() returned again.

  This implementation is intended to be used for *synchronous* context
  switches only.  Therefore, it does not have to save anything
  other than the PRESERVED state.  */


ENTRY(__getcontext)
    /* Save the preserved registers, the registers used for passing
       args, and the return address.  */
    movq    %rbx, oRBX(%rdi)
    movq    %rbp, oRBP(%rdi)
    movq    %r12, oR12(%rdi)
    movq    %r13, oR13(%rdi)
    movq    %r14, oR14(%rdi)
    movq    %r15, oR15(%rdi)

    movq    %rdi, oRDI(%rdi)
    movq    %rsi, oRSI(%rdi)
    movq    %rdx, oRDX(%rdi)
    movq    %rcx, oRCX(%rdi)
    movq    %r8, oR8(%rdi)
    movq    %r9, oR9(%rdi)

    movq    (%rsp), %rcx
    movq    %rcx, oRIP(%rdi)
    leaq    8(%rsp), %rcx       /* Exclude the return address.  */
    movq    %rcx, oRSP(%rdi)

    /* We have separate floating-point register content memory on the
       stack.  We use the __fpregs_mem block in the context.  Set the
       links up correctly.  */

    leaq    oFPREGSMEM(%rdi), %rcx
    movq    %rcx, oFPREGS(%rdi)
    /* Save the floating-point environment.  */
    fnstenv (%rcx)
    fldenv  (%rcx)
    stmxcsr oMXCSR(%rdi)

    /* Save the current signal mask with
       rt_sigprocmask (SIG_BLOCK, NULL, set,_NSIG/8).  */
    leaq    oSIGMASK(%rdi), %rdx
    xorl    %esi,%esi
#if SIG_BLOCK == 0
    xorl    %edi, %edi
#else
    movl    $SIG_BLOCK, %edi
#endif
    movl    $_NSIG8,%r10d
    movl    $__NR_rt_sigprocmask, %eax
    syscall
    cmpq    $-4095, %rax        /* Check %rax for error.  */
    jae SYSCALL_ERROR_LABEL /* Jump to error handler if error.  */

    /* All done, return 0 for success.  */
    xorl    %eax, %eax
    ret
PSEUDO_END(__getcontext)

weak_alias (__getcontext, getcontext)
```
这段代码主要执行了几个工作，将当前的寄存器数据存入到%rdi也就是第一个参数ucp中，紧接着调整栈顶指针%rsp。然后设置浮点计算器，保存当前线程的信号屏蔽掩码。

-----------
## setcontext
```c
int setcontext(const ucontext_t *ucp);
```
将变量ucp中保存的寄存器信息恢复到CPU中。

汇编代码
```c
/*  int __setcontext (const ucontext_t *ucp)

  Restores the machine context in UCP and thereby resumes execution
  in that context.

  This implementation is intended to be used for *synchronous* context
  switches only.  Therefore, it does not have to restore anything
  other than the PRESERVED state.  */

ENTRY(__setcontext)
    /* Save argument since syscall will destroy it.  */
    pushq   %rdi
    cfi_adjust_cfa_offset(8)

    /* Set the signal mask with
       rt_sigprocmask (SIG_SETMASK, mask, NULL, _NSIG/8).  */
    leaq    oSIGMASK(%rdi), %rsi
    xorl    %edx, %edx
    movl    $SIG_SETMASK, %edi
    movl    $_NSIG8,%r10d
    movl    $__NR_rt_sigprocmask, %eax
    syscall
    popq    %rdi            /* Reload %rdi, adjust stack.  */
    cfi_adjust_cfa_offset(-8)
    cmpq    $-4095, %rax        /* Check %rax for error.  */
    jae SYSCALL_ERROR_LABEL /* Jump to error handler if error.  */

    /* Restore the floating-point context.  Not the registers, only the
       rest.  */
    movq    oFPREGS(%rdi), %rcx
    fldenv  (%rcx)
    ldmxcsr oMXCSR(%rdi)


    /* Load the new stack pointer, the preserved registers and
       registers used for passing args.  */
    cfi_def_cfa(%rdi, 0)
    cfi_offset(%rbx,oRBX)
    cfi_offset(%rbp,oRBP)
    cfi_offset(%r12,oR12)
    cfi_offset(%r13,oR13)
    cfi_offset(%r14,oR14)
    cfi_offset(%r15,oR15)
    cfi_offset(%rsp,oRSP)
    cfi_offset(%rip,oRIP)

    movq    oRSP(%rdi), %rsp
    movq    oRBX(%rdi), %rbx
    movq    oRBP(%rdi), %rbp
    movq    oR12(%rdi), %r12
    movq    oR13(%rdi), %r13
    movq    oR14(%rdi), %r14
    movq    oR15(%rdi), %r15

    /* The following ret should return to the address set with
    getcontext.  Therefore push the address on the stack.  */
    movq    oRIP(%rdi), %rcx
    pushq   %rcx

    movq    oRSI(%rdi), %rsi
    movq    oRDX(%rdi), %rdx
    movq    oRCX(%rdi), %rcx
    movq    oR8(%rdi), %r8
    movq    oR9(%rdi), %r9

    /* Setup finally  %rdi.  */
    movq    oRDI(%rdi), %rdi

    /* End FDE here, we fall into another context.  */
    cfi_endproc
    cfi_startproc

    /* Clear rax to indicate success.  */
    xorl    %eax, %eax
    ret
PSEUDO_END(__setcontext)
```
setcontext的操作与getcontext类似，他将ucp中所保存的上下文信息给取出来，放入当前的寄存器中，使得当前的上下文环境恢复的与ucp一致

-------
## makecontext

```c
void makecontext(ucontext_t *ucp, void (*func)(), int argc, ...);
```

修改上下文信息，设置上下文入口函数func，agrc为参数个数，后面跟着的函数参数必须要是整型值。并且在makecontext之前，需要为上下文设置栈空间ucp->stack以及设置后继上下文ucp->uc_link。

汇编代码

```c
 void
  __makecontext (ucontext_t *ucp, void (*func) (void), int argc, ...)
  {
    extern void __start_context (void);
    greg_t *sp;
    unsigned int idx_uc_link;
    va_list ap; 
    int i;
  
    /* Generate room on stack for parameter if needed and uc_link.  */
    sp = (greg_t *) ((uintptr_t) ucp->uc_stack.ss_sp
             + ucp->uc_stack.ss_size);
    sp -= (argc > 6 ? argc - 6 : 0) + 1;
    /* Align stack and make space for trampoline address.  */
    sp = (greg_t *) ((((uintptr_t) sp) & -16L) - 8); 
  
    idx_uc_link = (argc > 6 ? argc - 6 : 0) + 1;
  
    /* Setup context ucp.  */
    /* Address to jump to.  */
    ucp->uc_mcontext.gregs[REG_RIP] = (uintptr_t) func;
    /* Setup rbx.*/
    ucp->uc_mcontext.gregs[REG_RBX] = (uintptr_t) &sp[idx_uc_link];
    ucp->uc_mcontext.gregs[REG_RSP] = (uintptr_t) sp; 
  
    /* Setup stack.  */
    sp[0] = (uintptr_t) &__start_context;
    sp[idx_uc_link] = (uintptr_t) ucp->uc_link;
  
    va_start (ap, argc);
    /* Handle arguments.
  
       The standard says the parameters must all be int values.  This is
       an historic accident and would be done differently today.  For
       x86-64 all integer values are passed as 64-bit values and
       therefore extending the API to copy 64-bit values instead of
       32-bit ints makes sense.  It does not break existing
       functionality and it does not violate the standard which says
       that passing non-int values means undefined behavior.  */
    for (i = 0; i < argc; ++i)
      switch (i)
        {
        case 0:
      ucp->uc_mcontext.gregs[REG_RDI] = va_arg (ap, greg_t);
      break;
        case 1:
      ucp->uc_mcontext.gregs[REG_RSI] = va_arg (ap, greg_t);
      break;
        case 2:
      ucp->uc_mcontext.gregs[REG_RDX] = va_arg (ap, greg_t);
      break;
        case 3:
      ucp->uc_mcontext.gregs[REG_RCX] = va_arg (ap, greg_t);
      break;
        case 4:
      ucp->uc_mcontext.gregs[REG_R8] = va_arg (ap, greg_t);
      break;
        case 5:
      ucp->uc_mcontext.gregs[REG_R9] = va_arg (ap, greg_t);
      break;
        default:
      /* Put value on stack.  */
      sp[i - 5] = va_arg (ap, greg_t);
      break;
        }
    va_end (ap);
  }
```
这段代码的主要内容其实就是为用户的自定义栈进行处理，将**当前运行栈切换为用户的自定义栈**，并且将用户传入的入口函数放入rip中，rbx指向后继上下文，rsp指向栈顶。

```c
ENTRY(__start_context)
    /* This removes the parameters passed to the function given to
       'makecontext' from the stack.  RBX contains the address
       on the stack pointer for the next context.  */
    movq    %rbx, %rsp

    /* Don't use pop here so that stack is aligned to 16 bytes.  */
    movq    (%rsp), %rdi        /* This is the next context.  */
    testq   %rdi, %rdi
    je  2f          /* If it is zero exit.  */

    call    __setcontext
    /* If this returns (which can happen if the syscall fails) we'll
       exit the program with the return error value (-1).  */
    movq    %rax,%rdi

2:
    call    HIDDEN_JUMPTARGET(exit)
    /* The 'exit' call should never return.  In case it does cause
       the process to terminate.  */
    hlt 
END(__start_context)
```
makecontext通过调用__start_context()来实现后继上下文的功能，其实就是将后继上下文作为setcontext的参数，调用setcontext将当前上下文设置到后继上下文的状态

-----------
## swapcontext

```c
int swapcontext(ucontext_t *oucp, const ucontext_t *ucp);
```

切换上下文，保存当前上下文到oucp中，然后激活ucp中的上下文

汇编代码

```c
/* int __swapcontext (ucontext_t *oucp, const ucontext_t *ucp);

  Saves the machine context in oucp such that when it is activated,
  it appears as if __swapcontextt() returned again, restores the
  machine context in ucp and thereby resumes execution in that
  context.

  This implementation is intended to be used for *synchronous* context
  switches only.  Therefore, it does not have to save anything
  other than the PRESERVED state.  */

ENTRY(__swapcontext)
    /* Save the preserved registers, the registers used for passing args,
       and the return address.  */
    movq    %rbx, oRBX(%rdi)
    movq    %rbp, oRBP(%rdi)
    movq    %r12, oR12(%rdi)
    movq    %r13, oR13(%rdi)
    movq    %r14, oR14(%rdi)
    movq    %r15, oR15(%rdi)

    movq    %rdi, oRDI(%rdi)
    movq    %rsi, oRSI(%rdi)
    movq    %rdx, oRDX(%rdi)
    movq    %rcx, oRCX(%rdi)
    movq    %r8, oR8(%rdi)
    movq    %r9, oR9(%rdi)

    movq    (%rsp), %rcx
    movq    %rcx, oRIP(%rdi)
    leaq    8(%rsp), %rcx       /* Exclude the return address.  */
    movq    %rcx, oRSP(%rdi)

    /* We have separate floating-point register content memory on the
       stack.  We use the __fpregs_mem block in the context.  Set the
       links up correctly.  */
    leaq    oFPREGSMEM(%rdi), %rcx
    movq    %rcx, oFPREGS(%rdi)
    /* Save the floating-point environment.  */
    fnstenv (%rcx)
    stmxcsr oMXCSR(%rdi)


    /* The syscall destroys some registers, save them.  */
    movq    %rsi, %r12

    /* Save the current signal mask and install the new one with
       rt_sigprocmask (SIG_BLOCK, newset, oldset,_NSIG/8).  */
    leaq    oSIGMASK(%rdi), %rdx
    leaq    oSIGMASK(%rsi), %rsi
    movl    $SIG_SETMASK, %edi
    movl    $_NSIG8,%r10d
    movl    $__NR_rt_sigprocmask, %eax
    syscall
    cmpq    $-4095, %rax        /* Check %rax for error.  */
    jae SYSCALL_ERROR_LABEL /* Jump to error handler if error.  */

    /* Restore destroyed registers.  */
    movq    %r12, %rsi

    /* Restore the floating-point context.  Not the registers, only the
       rest.  */
    movq    oFPREGS(%rsi), %rcx
    fldenv  (%rcx)
    ldmxcsr oMXCSR(%rsi)

    /* Load the new stack pointer and the preserved registers.  */
    movq    oRSP(%rsi), %rsp
    movq    oRBX(%rsi), %rbx
    movq    oRBP(%rsi), %rbp
    movq    oR12(%rsi), %r12
    movq    oR13(%rsi), %r13
    movq    oR14(%rsi), %r14
    movq    oR15(%rsi), %r15

    /* The following ret should return to the address set with
    getcontext.  Therefore push the address on the stack.  */
    movq    oRIP(%rsi), %rcx
    pushq   %rcx

    /* Setup registers used for passing args.  */
    movq    oRDI(%rsi), %rdi
    movq    oRDX(%rsi), %rdx
    movq    oRCX(%rsi), %rcx
    movq    oR8(%rsi), %r8
    movq    oR9(%rsi), %r9

    /* Setup finally  %rsi.  */
    movq    oRSI(%rsi), %rsi

    /* Clear rax to indicate success.  */
    xorl    %eax, %eax
    ret
PSEUDO_END(__swapcontext)
```
从汇编代码可以看出来，swapcontext的主要操作其实就是整合了getcontext和setcontext。首先将当前的上下文环境保存到ousp中，紧接着将当前的上下文环境设置为usp中的上下文环境。

-----------
# 使用示例
## 示例一、上下文的保存与恢复(getcontext、setcontext)

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ucontext.h>

int main() 
{
	int i = 0;
	ucontext_t ctx;

	getcontext(&ctx);//在该位置保存上下文
	printf("i = %d\n", i++);
	sleep(2);
	setcontext(&ctx);//将上下文恢复至设置时的状态，完成死循环
	
	return 0;
}
```
通过在第三行的地方设置上下文，每次执行完计数后，将上下文环境恢复至getcontext的位置，实现循环计数。
![在这里插入图片描述](https://img-blog.csdnimg.cn/202008172118495.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzM1NDIzMTU0,size_16,color_FFFFFF,t_70#pic_center)
![在这里插入图片描述](https://img-blog.csdnimg.cn/20200817214317828.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzM1NDIzMTU0,size_16,color_FFFFFF,t_70#pic_center)

-------------
## 示例二、上下文的修改(makecontext)

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ucontext.h>

void fun( void ) {
	printf("fun()\n");
}


int main( void ) {
	int i = 1;
	char *stack = (char*)malloc(sizeof(char)*8192);
	ucontext_t ctx_main, ctx_fun;

	getcontext(&ctx_main);//保存ctx_main上下文
	getcontext(&ctx_fun);//保存ctx_fun上下文
	printf("i=%d\n", i++);
	sleep(1);

	//设置上下文的栈信息
	ctx_fun.uc_stack.ss_sp    = stack;
	ctx_fun.uc_stack.ss_size  = 8192;
	ctx_fun.uc_stack.ss_flags = 0;
	ctx_fun.uc_link = &ctx_main;//设置ctx_main为ctx_fun的后继上下文

	makecontext(&ctx_fun, fun, 0); // 修改上下文信息，设置入口函数与参数
	
	setcontext(&ctx_fun);//恢复ctx_fun上下文
	
	printf("main exit\n");
}
```
![在这里插入图片描述](https://img-blog.csdnimg.cn/20200817213803523.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzM1NDIzMTU0,size_16,color_FFFFFF,t_70#pic_center)
进入死循环，每次ctx_fun执行完fun函数后就会跳转到后继上下文ctx_main的保存位置的下一句，然后继续开始计数，当走到setcontext的时候再次跳转至fun函数，进入死循环，如下图。
![在这里插入图片描述](https://img-blog.csdnimg.cn/20200817214118492.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzM1NDIzMTU0,size_16,color_FFFFFF,t_70#pic_center)

--------------
## 示例三、上下文的切换(swapcontext)

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ucontext.h>

ucontext_t ctx_main, ctx_f1, ctx_f2;

void fun1( void ) {
	printf("fun1() start\n");
	swapcontext(&ctx_f1, &ctx_f2);//切换至f2上下文
	printf("fun1() end\n");
}

void fun2( void ) {
	printf("fun2() start\n");
	swapcontext(&ctx_f2, &ctx_f1);//切换回f1上下文
	printf("fun2() end\n");
}

int main( void ) {
	char stack1[1024*8];
	char stack2[1024*8];
	
	getcontext(&ctx_f1);
	getcontext(&ctx_f2);

	ctx_f1.uc_stack.ss_sp    = stack1;
	ctx_f1.uc_stack.ss_size  = 1024*8;
	ctx_f1.uc_stack.ss_flags = 0;
	ctx_f1.uc_link = &ctx_f2;//f1设置后继上下文为f2
	makecontext(&ctx_f1, fun1, 0);//设置入口函数

	ctx_f2.uc_stack.ss_sp    = stack2;
	ctx_f2.uc_stack.ss_size  = 1024*8;
	ctx_f2.uc_stack.ss_flags = 0;
	ctx_f2.uc_link = &ctx_main;//f2后继上下文为主流程
	makecontext(&ctx_f2, fun2, 0);//设置入口函数

	swapcontext(&ctx_main, &ctx_f1);//保存ctx_main,从主流程上下文切换至ctx_f1上下文

	printf("main exit\n");
}
```
![在这里插入图片描述](https://img-blog.csdnimg.cn/20200817214422777.png#pic_center)
首先从主流程ctx_main切换至ctx_fun1的入口函数fun1，执行完fun1 start后切换至ctx_fun2的入口函数fun2。接着执行fun2 start，然后再次切换回ctx_fun1，执行fun1 end，此时fun1上下文执行结束，跳转至后继上下文ctx_fun2，执行fun2 end。接着fun2也执行结束，跳转至后继上下文主流程ctx_main，执行main exit退出。
![在这里插入图片描述](https://img-blog.csdnimg.cn/20200817215300743.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzM1NDIzMTU0,size_16,color_FFFFFF,t_70#pic_center)