/**
 * fptrace_stack - 函数调用堆栈追踪
 * 
 * 功能：注册需要追踪的函数指针，当调用时自动打印调用堆栈并保存到文件
 * 
 * 使用流程：
 *   1. fptrace_stack_init("trace.log")     - 初始化，指定日志文件
 *   2. FPT_REGISTER(ops->read)             - 注册要追踪的函数指针
 *   3. FPT_TRACE(ops->read, buf, len)      - 调用时自动追踪
 *   4. fptrace_stack_cleanup()             - 清理资源
 * 
 * 编译选项：
 *   - NO_BACKTRACE: 禁用 backtrace()（某些嵌入式环境没有）
 * 
 * 依赖：
 *   - fptrace.h/fptrace.c（用于解析函数名）
 * 
 * GitHub: https://github.com/TbusOS/fptrace
 * License: MIT
 */

#ifndef FPTRACE_STACK_H
#define FPTRACE_STACK_H

#include "fptrace.h"  /* 依赖 fptrace_name() */

/*============================================================================
 * 堆栈追踪 API
 *============================================================================*/

/**
 * 初始化堆栈追踪系统
 * 
 * @param log_file 日志文件路径，NULL 表示输出到 stderr
 */
void fptrace_stack_init(const char *log_file);

/**
 * 注册要追踪的函数指针
 * 
 * @param func_ptr 函数指针
 * @param name 函数指针的名称（用于日志显示）
 */
void fptrace_stack_register(void *func_ptr, const char *name);

/**
 * 取消注册函数指针
 */
void fptrace_stack_unregister(void *func_ptr);

/**
 * 检查函数指针是否已注册，如果是则打印调用堆栈
 * 通常不直接调用，而是通过 FPT_TRACE 宏使用
 */
void fptrace_stack_check(void *func_ptr);

/**
 * 清理堆栈追踪系统资源
 */
void fptrace_stack_cleanup(void);

/**
 * 获取当前追踪统计信息
 * 
 * @param registered 输出：已注册的函数指针数量
 * @param traced 输出：已追踪的调用次数
 */
void fptrace_stack_stats(int *registered, int *traced);

/*============================================================================
 * 堆栈追踪宏
 *============================================================================*/

/**
 * 简化注册宏
 * 用法: FPT_REGISTER(ops->read);
 */
#define FPT_REGISTER(ptr) \
    fptrace_stack_register((void *)(ptr), #ptr)

/**
 * 调用函数指针并自动追踪（如果已注册）
 * 用法: FPT_TRACE(ops->read, buf, len);
 */
#define FPT_TRACE(func_ptr, ...) do { \
    fptrace_stack_check((void *)(func_ptr)); \
    (func_ptr)(__VA_ARGS__); \
} while(0)

/**
 * 带返回值的追踪调用
 * 用法: ret = FPT_TRACE_RET(ops->get_value, arg);
 */
#define FPT_TRACE_RET(func_ptr, ...) ({ \
    fptrace_stack_check((void *)(func_ptr)); \
    (func_ptr)(__VA_ARGS__); \
})

#endif /* FPTRACE_STACK_H */

