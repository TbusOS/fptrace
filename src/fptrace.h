/**
 * fptrace - Function Pointer Trace
 * 
 * 在运行时获取函数指针指向的函数名称
 * 
 * 用途：大型代码库中调试函数指针，查看回调函数实际指向谁
 * 
 * 特点：
 * - 纯代码实现，不依赖 addr2line、atos 等外部命令
 * - 自动处理 PIE/ASLR 地址随机化
 * - 支持嵌入式 Linux 环境
 * - 提供线程安全版本
 * 
 * 两种模式：
 * 
 * 1. 默认模式（使用 dladdr）
 *    编译: gcc your_code.c fptrace.c -ldl -o program
 *    条件: 需要 libdl（大多数 Linux 系统都有）
 *    优点: 可解析主程序和共享库的函数
 * 
 * 2. NO_DLADDR 模式（手动解析 ELF）
 *    编译: gcc -DNO_DLADDR your_code.c fptrace.c -o program
 *    条件（必须全部满足）:
 *      - Linux 系统
 *      - /proc/self/exe 可读
 *      - /proc/self/maps 可读
 *      - 可执行文件存在且可读
 *      - 可执行文件未被 strip
 *    限制: 只能解析主程序的函数，不能解析共享库
 * 
 * GitHub: https://github.com/TbusOS/fptrace
 * License: MIT
 */

#ifndef FPTRACE_H
#define FPTRACE_H

#include <stddef.h>

/*============================================================================
 * 基础 API
 *============================================================================*/

/**
 * 根据函数指针获取函数名称
 * 
 * @param func_ptr 函数指针
 * @return 函数名称字符串，无法解析返回 "(unknown)"
 *         返回的字符串不需要释放
 * 
 * 示例:
 *   void (*callback)(int) = my_handler;
 *   printf("callback -> %s\n", fptrace_name((void *)callback));
 *   // 输出: callback -> my_handler
 */
const char *fptrace_name(void *func_ptr);

/**
 * 打印函数指针详细信息
 * 
 * @param func_ptr 函数指针
 */
void fptrace_print(void *func_ptr);

/*============================================================================
 * 格式化 API
 *============================================================================*/

/**
 * 获取格式化字符串: "func_name (0x12345678)"
 * 注意：使用静态缓冲区，非线程安全
 */
const char *fptrace_fmt(void *func_ptr);

/*============================================================================
 * 线程安全 API
 *============================================================================*/

/**
 * 获取函数名称（线程安全，用户提供缓冲区）
 */
char *fptrace_name_r(void *func_ptr, char *buf, size_t buf_size);

/**
 * 获取格式化字符串（线程安全，用户提供缓冲区）
 */
char *fptrace_fmt_r(void *func_ptr, char *buf, size_t buf_size);

/**
 * 获取格式化字符串（线程安全，使用 TLS）
 */
const char *fptrace_fmt_tls(void *func_ptr);

/*============================================================================
 * 诊断 API
 *============================================================================*/

/**
 * 打印诊断信息（用于排查问题）
 * 显示 ELF 解析状态、符号表信息等
 */
void fptrace_debug(void);

/*============================================================================
 * 调试宏 - 方便快速打印
 *============================================================================*/

/**
 * 打印函数指针名称
 * 用法: FPT_PRINT(my_callback);
 * 输出: my_callback = some_handler (0x12345678)
 */
#define FPT_PRINT(ptr) \
    printf("%s = %s\n", #ptr, fptrace_fmt((void *)(ptr)))

/**
 * 在调用函数指针前打印信息
 * 用法: FPT_CALL(ops->callback, arg1, arg2);
 */
#define FPT_CALL(func_ptr, ...) do { \
    printf("[fptrace] %s -> %s\n", #func_ptr, fptrace_fmt((void *)(func_ptr))); \
    (func_ptr)(__VA_ARGS__); \
} while(0)

/**
 * 带返回值的版本
 * 用法: ret = FPT_CALL_RET(ops->get_value, arg);
 */
#define FPT_CALL_RET(func_ptr, ...) ({ \
    printf("[fptrace] %s -> %s\n", #func_ptr, fptrace_fmt((void *)(func_ptr))); \
    (func_ptr)(__VA_ARGS__); \
})

/*============================================================================
 * 函数调用堆栈追踪 API
 * 
 * 功能：注册需要追踪的函数指针，当调用时自动打印调用堆栈并保存到文件
 * 
 * 使用流程：
 *   1. fptrace_stack_init("trace.log")     - 初始化，指定日志文件
 *   2. FPT_REGISTER(ops->read)             - 注册要追踪的函数指针
 *   3. FPT_TRACE(ops->read, buf, len)      - 调用时自动追踪
 *   4. fptrace_stack_cleanup()             - 清理资源
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

#endif /* FPTRACE_H */
