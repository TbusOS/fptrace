/**
 * fptrace 堆栈追踪示例
 * 
 * 演示如何使用 fptrace 的堆栈追踪功能：
 * 1. 注册需要追踪的函数指针
 * 2. 调用时自动记录调用堆栈
 * 3. 保存到日志文件
 */

#include <stdio.h>
#include <stdlib.h>
#include "fptrace.h"

/*============================================================================
 * 模拟的回调函数
 *============================================================================*/

typedef int (*data_handler_t)(void *data, int len);
typedef void (*event_callback_t)(int event_type);

/* 处理函数 */
int handle_network_data(void *data, int len)
{
    printf("    [handle_network_data] 处理网络数据: %d 字节\n", len);
    (void)data;
    return len;
}

int handle_file_data(void *data, int len)
{
    printf("    [handle_file_data] 处理文件数据: %d 字节\n", len);
    (void)data;
    return len;
}

void on_connect_event(int event_type)
{
    printf("    [on_connect_event] 连接事件: %d\n", event_type);
}

void on_disconnect_event(int event_type)
{
    printf("    [on_disconnect_event] 断开事件: %d\n", event_type);
}

/*============================================================================
 * 模拟多层调用的函数
 *============================================================================*/

/* 第3层 - 实际调用回调 */
void layer3_invoke(data_handler_t handler, void *data, int len)
{
    printf("  [layer3] 调用处理器\n");
    FPT_TRACE(handler, data, len);  /* 自动追踪 */
}

/* 第2层 */
void layer2_process(data_handler_t handler, void *data, int len)
{
    printf("  [layer2] 处理请求\n");
    layer3_invoke(handler, data, len);
}

/* 第1层 */
void layer1_dispatch(data_handler_t handler, void *data, int len)
{
    printf("  [layer1] 分发请求\n");
    layer2_process(handler, data, len);
}

/*============================================================================
 * 事件系统模拟
 *============================================================================*/

struct event_system {
    event_callback_t on_connect;
    event_callback_t on_disconnect;
};

void emit_event(struct event_system *sys, int connected)
{
    if (connected && sys->on_connect) {
        FPT_TRACE(sys->on_connect, 1);
    } else if (!connected && sys->on_disconnect) {
        FPT_TRACE(sys->on_disconnect, 0);
    }
}

/*============================================================================
 * 主函数
 *============================================================================*/

int main(void)
{
    data_handler_t handlers[] = {
        handle_network_data,
        handle_file_data,
    };
    
    struct event_system events = {
        .on_connect = on_connect_event,
        .on_disconnect = on_disconnect_event,
    };
    
    int registered, traced;
    
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║   fptrace 堆栈追踪示例                                         ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n\n");
    
    /* 1. 初始化追踪系统，指定日志文件 */
    printf("=== 步骤1: 初始化追踪系统 ===\n");
    fptrace_stack_init("trace.log");
    printf("日志文件: trace.log\n\n");
    
    /* 2. 注册要追踪的函数指针 */
    printf("=== 步骤2: 注册要追踪的函数指针 ===\n");
    FPT_REGISTER(handlers[0]);
    FPT_REGISTER(handlers[1]);
    FPT_REGISTER(events.on_connect);
    FPT_REGISTER(events.on_disconnect);
    printf("\n");
    
    /* 3. 模拟多层调用 */
    printf("=== 步骤3: 模拟多层调用（会记录堆栈） ===\n\n");
    
    printf("[调用1] 通过多层函数调用 handler[0]:\n");
    layer1_dispatch(handlers[0], NULL, 100);
    printf("\n");
    
    printf("[调用2] 通过多层函数调用 handler[1]:\n");
    layer1_dispatch(handlers[1], NULL, 200);
    printf("\n");
    
    printf("[调用3] 再次调用 handler[0]:\n");
    layer1_dispatch(handlers[0], NULL, 300);
    printf("\n");
    
    /* 4. 事件系统调用 */
    printf("=== 步骤4: 事件系统调用 ===\n\n");
    
    printf("[事件1] 触发连接事件:\n");
    emit_event(&events, 1);
    printf("\n");
    
    printf("[事件2] 触发断开事件:\n");
    emit_event(&events, 0);
    printf("\n");
    
    /* 5. 获取统计信息 */
    printf("=== 步骤5: 统计信息 ===\n");
    fptrace_stack_stats(&registered, &traced);
    printf("已注册函数指针: %d 个\n", registered);
    printf("已追踪调用次数: %d 次\n", traced);
    printf("\n");
    
    /* 6. 清理并输出最终统计 */
    printf("=== 步骤6: 清理并保存统计 ===\n");
    fptrace_stack_cleanup();
    printf("追踪日志已保存到 trace.log\n");
    printf("\n");
    
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║   查看日志: cat trace.log                                      ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n");
    
    return 0;
}

