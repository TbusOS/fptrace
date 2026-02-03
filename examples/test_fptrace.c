/**
 * fptrace 使用示例
 * 
 * 展示如何使用 fptrace 库追踪函数指针
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fptrace.h"

/*============================================================================
 * 模拟大型代码库中的回调场景
 *============================================================================*/

/* 回调函数类型 */
typedef int (*data_handler_t)(void *data, int len);
typedef void (*event_callback_t)(int event_type);

/* 实际的处理函数 */
int process_network_data(void *data, int len)
{
    printf("    -> 处理网络数据: %d 字节\n", len);
    (void)data;
    return 0;
}

int process_file_data(void *data, int len)
{
    printf("    -> 处理文件数据: %d 字节\n", len);
    (void)data;
    return 0;
}

void on_connect(int event_type)
{
    printf("    -> 连接事件: %d\n", event_type);
}

void on_disconnect(int event_type)
{
    printf("    -> 断开事件: %d\n", event_type);
}

/* 模拟操作结构体（常见于驱动/框架） */
struct device_ops {
    data_handler_t    read;
    data_handler_t    write;
    event_callback_t  on_event;
};

/*============================================================================
 * 测试场景1：不知道函数指针指向谁
 *============================================================================*/
void test_scenario_unknown_callback(void)
{
    printf("=== 场景1: 调试未知的函数指针 ===\n\n");
    
    /* 模拟从某处获得一个函数指针，但不知道它指向谁 */
    data_handler_t handler = process_network_data;
    
    printf("问题：handler 指向哪个函数？\n");
    printf("答案：%s\n\n", fptrace_name((void *)handler));
    
    /* 使用宏更方便 */
    FPT_PRINT(handler);
    printf("\n");
}

/*============================================================================
 * 测试场景2：调用前追踪
 *============================================================================*/
void test_scenario_trace_call(void)
{
    printf("=== 场景2: 调用函数指针前追踪 ===\n\n");
    
    event_callback_t callback = on_connect;
    
    /* 方法1：手动打印 */
    printf("即将调用: %s\n", fptrace_fmt((void *)callback));
    callback(1);
    printf("\n");
    
    /* 方法2：使用 FPT_CALL 宏 */
    callback = on_disconnect;
    FPT_CALL(callback, 2);
    printf("\n");
}

/*============================================================================
 * 测试场景3：结构体中的函数指针
 *============================================================================*/
void test_scenario_ops_struct(void)
{
    printf("=== 场景3: 检查结构体中的函数指针 ===\n\n");
    
    struct device_ops ops = {
        .read     = process_network_data,
        .write    = process_file_data,
        .on_event = on_connect,
    };
    
    printf("ops 结构体的函数指针:\n");
    printf("  ops.read     -> %s\n", fptrace_name((void *)ops.read));
    printf("  ops.write    -> %s\n", fptrace_name((void *)ops.write));
    printf("  ops.on_event -> %s\n", fptrace_name((void *)ops.on_event));
    printf("\n");
    
    /* 或者用宏 */
    FPT_PRINT(ops.read);
    FPT_PRINT(ops.write);
    FPT_PRINT(ops.on_event);
    printf("\n");
}

/*============================================================================
 * 测试场景4：动态改变的回调
 *============================================================================*/
void call_handler(const char *name, data_handler_t handler, void *data, int len)
{
    printf("[%s] 调用处理器: %s\n", name, fptrace_fmt((void *)handler));
    handler(data, len);
}

void test_scenario_dynamic_callback(void)
{
    printf("=== 场景4: 追踪动态变化的回调 ===\n\n");
    
    data_handler_t handlers[] = {
        process_network_data,
        process_file_data,
        process_network_data,
    };
    
    for (int i = 0; i < 3; i++) {
        char name[32];
        snprintf(name, sizeof(name), "第%d次调用", i + 1);
        call_handler(name, handlers[i], NULL, 100 + i);
    }
    printf("\n");
}

/*============================================================================
 * 测试场景5：检查库函数
 *============================================================================*/
void test_scenario_lib_functions(void)
{
    printf("=== 场景5: 解析库函数 ===\n\n");
    
    printf("printf  -> %s\n", fptrace_name((void *)printf));
    printf("malloc  -> %s\n", fptrace_name((void *)malloc));
    printf("free    -> %s\n", fptrace_name((void *)free));
    printf("strlen  -> %s\n", fptrace_name((void *)strlen));
    printf("\n");
}

/*============================================================================
 * 主函数
 *============================================================================*/
int main(void)
{
    printf("╔════════════════════════════════════════════════════╗\n");
    printf("║   fptrace - Function Pointer Trace 示例            ║\n");
    printf("╚════════════════════════════════════════════════════╝\n\n");
    
    /* 先打印诊断信息 */
    fptrace_debug();
    
    test_scenario_unknown_callback();
    test_scenario_trace_call();
    test_scenario_ops_struct();
    test_scenario_dynamic_callback();
    test_scenario_lib_functions();
    
    printf("API 列表:\n");
    printf("  fptrace_name(ptr)    - 获取函数名\n");
    printf("  fptrace_fmt(ptr)     - 获取 \"name (addr)\" 格式\n");
    printf("  fptrace_print(ptr)   - 打印详细信息\n");
    printf("  FPT_PRINT(ptr)       - 宏: 快速打印\n");
    printf("  FPT_CALL(ptr, args)  - 宏: 调用前追踪\n");
    
    return 0;
}
