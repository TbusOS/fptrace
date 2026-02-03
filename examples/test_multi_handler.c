/**
 * fptrace 多函数指针堆栈追踪示例
 * 
 * 演示同时追踪多个函数指针的调用堆栈
 */

#include <stdio.h>
#include "fptrace_stack.h"  /* 堆栈追踪功能（已包含 fptrace.h） */

/*============================================================================
 * 定义两个处理函数
 *============================================================================*/

typedef void (*handler_t)(int id);

void handler_A(int id)
{
    printf("  >>> handler_A 被调用, id=%d\n", id);
}

void handler_B(int id)
{
    printf("  >>> handler_B 被调用, id=%d\n", id);
}

/*============================================================================
 * 模拟调用链
 *============================================================================*/

/* 第3层：实际调用 handler */
void do_call(handler_t h, int id)
{
    printf("  [do_call] 准备调用 handler\n");
    FPT_TRACE(h, id);  /* 自动追踪堆栈 */
}

/* 第2层 */
void process(handler_t h, int id)
{
    printf("  [process] 处理中...\n");
    do_call(h, id);
}

/* 第1层 */
void dispatch(handler_t h, int id)
{
    printf("  [dispatch] 分发任务\n");
    process(h, id);
}

/*============================================================================
 * 主函数
 *============================================================================*/

int main(void)
{
    handler_t h1 = handler_A;
    handler_t h2 = handler_B;
    
    printf("=== fptrace 多 Handler 堆栈追踪 Demo ===\n\n");
    
    /* 1. 初始化，日志输出到 multi_trace.log */
    fptrace_stack_init("multi_trace.log");
    
    /* 2. 注册两个函数指针 */
    printf("[注册] 注册 handler_A 和 handler_B\n");
    FPT_REGISTER(h1);
    FPT_REGISTER(h2);
    printf("\n");
    
    /* 3. 调用 handler_A */
    printf("[调用1] 通过 dispatch -> process -> do_call 调用 handler_A:\n");
    dispatch(h1, 100);
    printf("\n");
    
    /* 4. 调用 handler_B */
    printf("[调用2] 通过 dispatch -> process -> do_call 调用 handler_B:\n");
    dispatch(h2, 200);
    printf("\n");
    
    /* 5. 再次调用 handler_A */
    printf("[调用3] 再次调用 handler_A:\n");
    dispatch(h1, 300);
    printf("\n");
    
    /* 6. 清理 */
    printf("[完成] 查看日志: cat multi_trace.log\n");
    fptrace_stack_cleanup();
    
    return 0;
}

