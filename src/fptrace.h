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

#endif /* FPTRACE_H */
