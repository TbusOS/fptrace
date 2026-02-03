/**
 * fptrace_stack - 函数调用堆栈追踪实现
 * 
 * 功能：注册需要追踪的函数指针，当调用时自动打印调用堆栈并保存到文件
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "fptrace_stack.h"

#ifndef NO_BACKTRACE
#include <execinfo.h>  /* backtrace, backtrace_symbols */
#endif

/*============================================================================
 * 配置常量
 *============================================================================*/

#define FPTRACE_MAX_REGISTERED   64   /* 最大注册数量 */
#define FPTRACE_MAX_STACK_DEPTH  32   /* 最大堆栈深度 */

/*============================================================================
 * 内部数据结构
 *============================================================================*/

/* 注册表项 */
struct fptrace_entry {
    void       *func_ptr;
    const char *name;
    int         call_count;
};

/* 追踪系统状态 */
static struct {
    int                   initialized;
    FILE                 *log_fp;
    int                   count;
    int                   total_traced;
    struct fptrace_entry  entries[FPTRACE_MAX_REGISTERED];
} fptrace_stack_ctx = {0};

/*============================================================================
 * API 实现
 *============================================================================*/

void fptrace_stack_init(const char *log_file)
{
    time_t now;
    
    if (fptrace_stack_ctx.initialized) {
        return;
    }
    
    fptrace_stack_ctx.initialized = 1;
    fptrace_stack_ctx.count = 0;
    fptrace_stack_ctx.total_traced = 0;
    
    if (log_file) {
        fptrace_stack_ctx.log_fp = fopen(log_file, "a");
        if (!fptrace_stack_ctx.log_fp) {
            fprintf(stderr, "[fptrace_stack] 警告: 无法打开日志文件 %s，使用 stderr\n", log_file);
            fptrace_stack_ctx.log_fp = stderr;
        }
    } else {
        fptrace_stack_ctx.log_fp = stderr;
    }
    
    /* 写入启动时间 */
    now = time(NULL);
    fprintf(fptrace_stack_ctx.log_fp, 
            "\n================================================================================\n");
    fprintf(fptrace_stack_ctx.log_fp,
            "fptrace 堆栈追踪启动: %s", ctime(&now));
    fprintf(fptrace_stack_ctx.log_fp,
            "================================================================================\n\n");
    fflush(fptrace_stack_ctx.log_fp);
}

void fptrace_stack_register(void *func_ptr, const char *name)
{
    int i;
    
    if (!fptrace_stack_ctx.initialized) {
        fptrace_stack_init(NULL);
    }
    
    if (fptrace_stack_ctx.count >= FPTRACE_MAX_REGISTERED) {
        fprintf(stderr, "[fptrace_stack] 警告: 注册表已满 (%d)，无法注册 %s\n", 
                FPTRACE_MAX_REGISTERED, name);
        return;
    }
    
    /* 检查是否已注册 */
    for (i = 0; i < fptrace_stack_ctx.count; i++) {
        if (fptrace_stack_ctx.entries[i].func_ptr == func_ptr) {
            return;  /* 已注册，跳过 */
        }
    }
    
    fptrace_stack_ctx.entries[fptrace_stack_ctx.count].func_ptr = func_ptr;
    fptrace_stack_ctx.entries[fptrace_stack_ctx.count].name = name;
    fptrace_stack_ctx.entries[fptrace_stack_ctx.count].call_count = 0;
    fptrace_stack_ctx.count++;
    
    fprintf(fptrace_stack_ctx.log_fp, 
            "[注册 #%d] %s -> %s (%p)\n", 
            fptrace_stack_ctx.count,
            name, fptrace_name(func_ptr), func_ptr);
    fflush(fptrace_stack_ctx.log_fp);
}

void fptrace_stack_unregister(void *func_ptr)
{
    int i;
    
    for (i = 0; i < fptrace_stack_ctx.count; i++) {
        if (fptrace_stack_ctx.entries[i].func_ptr == func_ptr) {
            /* 用最后一个覆盖当前位置 */
            fptrace_stack_ctx.entries[i] = 
                fptrace_stack_ctx.entries[fptrace_stack_ctx.count - 1];
            fptrace_stack_ctx.count--;
            return;
        }
    }
}

void fptrace_stack_check(void *func_ptr)
{
    const char *name = NULL;
    int entry_idx = -1;
    int i;
    FILE *fp;
    struct timespec ts;
#ifndef NO_BACKTRACE
    void *stack[FPTRACE_MAX_STACK_DEPTH];
    int depth;
    char **symbols = NULL;
#endif
    
    if (!fptrace_stack_ctx.initialized) {
        return;
    }
    
    /* 查找是否注册 */
    for (i = 0; i < fptrace_stack_ctx.count; i++) {
        if (fptrace_stack_ctx.entries[i].func_ptr == func_ptr) {
            name = fptrace_stack_ctx.entries[i].name;
            entry_idx = i;
            break;
        }
    }
    
    if (!name) {
        return;  /* 未注册，不追踪 */
    }
    
    /* 更新统计 */
    fptrace_stack_ctx.entries[entry_idx].call_count++;
    fptrace_stack_ctx.total_traced++;
    
    fp = fptrace_stack_ctx.log_fp;
    
    /* 获取时间戳 */
    clock_gettime(CLOCK_MONOTONIC, &ts);
    
    /* 输出堆栈信息 */
    fprintf(fp, "\n┌─────────────────────────────────────────────────────────────────────────────\n");
    fprintf(fp, "│ [调用 #%d] %s -> %s (%p)\n", 
            fptrace_stack_ctx.total_traced,
            name, fptrace_name(func_ptr), func_ptr);
    fprintf(fp, "│ 时间戳: %ld.%06ld  (第 %d 次调用此函数)\n", 
            (long)ts.tv_sec, ts.tv_nsec / 1000,
            fptrace_stack_ctx.entries[entry_idx].call_count);
    fprintf(fp, "├─────────────────────────────────────────────────────────────────────────────\n");

#ifndef NO_BACKTRACE
    /* 获取调用堆栈 */
    depth = backtrace(stack, FPTRACE_MAX_STACK_DEPTH);
    
    if (depth <= 2) {
        /* backtrace() 失败或返回太少的栈帧 */
        fprintf(fp, "│ 调用堆栈: (无法获取，depth=%d)\n", depth);
        fprintf(fp, "│ \n");
        fprintf(fp, "│ 提示: ARM32 平台需要特殊编译选项:\n");
        fprintf(fp, "│   -fno-omit-frame-pointer  (保留帧指针)\n");
        fprintf(fp, "│   -mapcs-frame 或 -marm    (ARM32 专用)\n");
        fprintf(fp, "│   -O0 或 -Og               (降低优化级别)\n");
    } else {
        symbols = backtrace_symbols(stack, depth);
        
        fprintf(fp, "│ 调用堆栈 (深度 %d):\n", depth - 2);
        
        for (i = 2; i < depth; i++) {  /* 跳过 fptrace_stack_check 和调用者 */
            const char *fname = fptrace_name(stack[i]);
            fprintf(fp, "│   #%-2d %s (%p)\n", i - 2, fname, stack[i]);
            
            /* 如果 fptrace 解析失败，显示 backtrace_symbols 的结果作为补充 */
            if (symbols && strcmp(fname, "(unknown)") == 0) {
                fprintf(fp, "│       └── %s\n", symbols[i]);
            }
        }
        
        if (symbols) {
            free(symbols);
        }
    }
#else
    fprintf(fp, "│ (堆栈追踪不可用 - NO_BACKTRACE 模式)\n");
#endif

    fprintf(fp, "└─────────────────────────────────────────────────────────────────────────────\n");
    fflush(fp);
}

void fptrace_stack_stats(int *registered, int *traced)
{
    if (registered) {
        *registered = fptrace_stack_ctx.count;
    }
    if (traced) {
        *traced = fptrace_stack_ctx.total_traced;
    }
}

void fptrace_stack_cleanup(void)
{
    int i;
    FILE *fp;
    time_t now;
    
    if (!fptrace_stack_ctx.initialized) {
        return;
    }
    
    fp = fptrace_stack_ctx.log_fp;
    now = time(NULL);
    
    /* 输出统计信息 */
    fprintf(fp, "\n================================================================================\n");
    fprintf(fp, "fptrace 堆栈追踪结束: %s", ctime(&now));
    fprintf(fp, "================================================================================\n");
    fprintf(fp, "统计信息:\n");
    fprintf(fp, "  已注册函数指针: %d 个\n", fptrace_stack_ctx.count);
    fprintf(fp, "  总追踪调用次数: %d 次\n", fptrace_stack_ctx.total_traced);
    fprintf(fp, "\n各函数调用统计:\n");
    
    for (i = 0; i < fptrace_stack_ctx.count; i++) {
        fprintf(fp, "  %-30s -> %-20s : %d 次\n",
                fptrace_stack_ctx.entries[i].name,
                fptrace_name(fptrace_stack_ctx.entries[i].func_ptr),
                fptrace_stack_ctx.entries[i].call_count);
    }
    
    fprintf(fp, "================================================================================\n\n");
    fflush(fp);
    
    /* 关闭文件 */
    if (fp && fp != stderr && fp != stdout) {
        fclose(fp);
    }
    
    fptrace_stack_ctx.initialized = 0;
    fptrace_stack_ctx.log_fp = NULL;
    fptrace_stack_ctx.count = 0;
    fptrace_stack_ctx.total_traced = 0;
}

