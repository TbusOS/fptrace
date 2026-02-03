# fptrace_stack 技术原理

本文档详细介绍 fptrace_stack 模块调用堆栈追踪的底层实现原理。

## 目录

1. [核心问题](#核心问题)
2. [堆栈追踪原理](#堆栈追踪原理)
3. [栈帧结构](#栈帧结构)
4. [backtrace() 工作机制](#backtrace-工作机制)
5. [注册表机制](#注册表机制)
6. [时间戳与统计](#时间戳与统计)
7. [日志输出格式](#日志输出格式)
8. [NO_BACKTRACE 模式](#no_backtrace-模式)
9. [常见问题](#常见问题)

---

## 核心问题

**问题**：当一个函数指针被调用时，如何知道是从哪里调用过来的？

**答案**：遍历调用栈（Call Stack），获取每一层的返回地址。

```
main() 调用 dispatch() 调用 process() 调用 handler()
                                              │
                                              ▼
                                    handler 被调用时，
                                    可以追溯完整的调用链
```

---

## 堆栈追踪原理

### 调用栈概念

当函数 A 调用函数 B 时：

```
1. A 把返回地址压入栈
2. A 把参数压入栈（或放入寄存器）
3. 跳转到 B 的代码
4. B 执行完后，从栈中取出返回地址，跳回 A
```

```
┌─────────────────────────────────────────────────────────────┐
│                    内存布局（栈向下增长）                      │
├─────────────────────────────────────────────────────────────┤
│  高地址                                                      │
│  ┌─────────────────────────────────────────────────────────┐│
│  │                    main() 的栈帧                         ││
│  │  ┌─────────────────────────────────────────────────────┐││
│  │  │  局部变量                                            │││
│  │  │  保存的寄存器                                        │││
│  │  │  返回地址 → __libc_start_main                       │││
│  │  │  旧的帧指针 (FP/RBP)                                 │││
│  │  └─────────────────────────────────────────────────────┘││
│  ├─────────────────────────────────────────────────────────┤│
│  │                  dispatch() 的栈帧                       ││
│  │  ┌─────────────────────────────────────────────────────┐││
│  │  │  局部变量                                            │││
│  │  │  保存的寄存器                                        │││
│  │  │  返回地址 → main+0x50                               │││
│  │  │  旧的帧指针 (FP/RBP)                                 │││
│  │  └─────────────────────────────────────────────────────┘││
│  ├─────────────────────────────────────────────────────────┤│
│  │                   process() 的栈帧                       ││
│  │  ┌─────────────────────────────────────────────────────┐││
│  │  │  局部变量                                            │││
│  │  │  保存的寄存器                                        │││
│  │  │  返回地址 → dispatch+0x30                           │││
│  │  │  旧的帧指针 (FP/RBP)                                 │││
│  │  └─────────────────────────────────────────────────────┘││
│  ├─────────────────────────────────────────────────────────┤│
│  │               fptrace_stack_check() 的栈帧              ││
│  │  ┌─────────────────────────────────────────────────────┐││
│  │  │  局部变量 (stack[], symbols, ...)                   │││
│  │  │  返回地址 → process+0x20                            │││  ← 当前位置
│  │  │  旧的帧指针 (FP/RBP)                                 │││
│  │  └─────────────────────────────────────────────────────┘││
│  └─────────────────────────────────────────────────────────┘│
│  低地址                                                      │
└─────────────────────────────────────────────────────────────┘
```

### 追踪方法

通过遍历帧指针链（Frame Pointer Chain）获取每一层的返回地址：

```
FP → [旧FP, 返回地址] → [旧FP, 返回地址] → [旧FP, 返回地址] → NULL
      process           dispatch          main
```

---

## 栈帧结构

### x86-64 架构

```
┌──────────────────────────────────┐  高地址
│  参数 N (如果超过6个)             │
│  ...                             │
│  参数 7                          │
├──────────────────────────────────┤
│  返回地址 (8 字节)                │  ← RBP+8
├──────────────────────────────────┤
│  旧的 RBP (8 字节)                │  ← RBP 指向这里
├──────────────────────────────────┤
│  局部变量                         │
│  ...                             │
├──────────────────────────────────┤
│  被保存的寄存器                   │
├──────────────────────────────────┤  低地址
│  (下一个函数的栈帧)               │  ← RSP
└──────────────────────────────────┘
```

### ARM32 架构

```
┌──────────────────────────────────┐  高地址
│  参数（如果栈传递）               │
├──────────────────────────────────┤
│  返回地址 LR (4 字节)             │  ← FP+4
├──────────────────────────────────┤
│  旧的 FP (4 字节)                 │  ← FP 指向这里
├──────────────────────────────────┤
│  局部变量                         │
│  ...                             │
├──────────────────────────────────┤  低地址
│  (下一个函数的栈帧)               │  ← SP
└──────────────────────────────────┘
```

### 编译器优化的影响

```bash
# 不优化，保留帧指针
gcc -O0 -fno-omit-frame-pointer code.c

# 优化后可能省略帧指针
gcc -O2 code.c  # 默认 -fomit-frame-pointer
```

**重要**：如果编译时使用 `-fomit-frame-pointer`，帧指针链会断开，backtrace() 可能无法正确工作。

---

## backtrace() 工作机制

### 函数原型

```c
#include <execinfo.h>

// 获取调用堆栈
int backtrace(void **buffer, int size);

// 将地址转换为符号字符串
char **backtrace_symbols(void **buffer, int size);

// 直接输出到文件描述符
void backtrace_symbols_fd(void **buffer, int size, int fd);
```

### 工作流程

```c
void *stack[32];
int depth = backtrace(stack, 32);

// backtrace() 内部实现（伪代码）：
int backtrace(void **buffer, int size) {
    void *fp = __builtin_frame_address(0);  // 获取当前帧指针
    int count = 0;
    
    while (fp != NULL && count < size) {
        // 从栈帧中提取返回地址
        void *return_addr = *(void **)((char *)fp + RETURN_ADDR_OFFSET);
        buffer[count++] = return_addr;
        
        // 移动到上一个栈帧
        fp = *(void **)fp;
        
        // 安全检查：避免无限循环
        if (!is_valid_address(fp)) break;
    }
    
    return count;
}
```

### 返回地址数组

```
stack[0] = backtrace() 自身的返回地址
stack[1] = 调用 backtrace() 的函数的返回地址
stack[2] = 再上一层...
...
stack[depth-1] = 最顶层（如 _start 或 __libc_start_main）
```

### 符号解析

```c
char **symbols = backtrace_symbols(stack, depth);

// 输出格式:
// "./program(func_name+0x1a) [0x401234]"
// "./program() [0x401234]"  （如果无符号信息）

for (int i = 0; i < depth; i++) {
    printf("%s\n", symbols[i]);
}

free(symbols);  // 需要手动释放
```

---

## 注册表机制

### 数据结构

```c
#define FPTRACE_MAX_REGISTERED 64

struct fptrace_entry {
    void       *func_ptr;    // 函数指针
    const char *name;        // 注册时的名称
    int         call_count;  // 调用计数
};

static struct {
    int                   initialized;   // 是否已初始化
    FILE                 *log_fp;        // 日志文件
    int                   count;         // 已注册数量
    int                   total_traced;  // 总追踪次数
    struct fptrace_entry  entries[FPTRACE_MAX_REGISTERED];
} fptrace_stack_ctx = {0};
```

### 注册流程

```
FPT_REGISTER(ops->read)
       │
       ▼
┌──────────────────────────────────────────────────────────────┐
│ fptrace_stack_register((void*)ops->read, "ops->read")       │
└──────────────────────────────────────────────────────────────┘
       │
       ▼
┌──────────────────────────────────────────────────────────────┐
│ 1. 检查是否已初始化，否则自动初始化                           │
│ 2. 检查注册表是否已满 (64 个)                                │
│ 3. 检查是否已注册（避免重复）                                 │
│ 4. 添加到 entries[] 数组                                     │
│ 5. 写入日志: "[注册 #1] ops->read -> usb_read (0x10880)"     │
└──────────────────────────────────────────────────────────────┘
```

### 查找机制

```c
// FPT_TRACE(ops->read, buf, len) 展开为:
do {
    fptrace_stack_check((void *)(ops->read));
    (ops->read)(buf, len);
} while(0)

// fptrace_stack_check() 内部:
void fptrace_stack_check(void *func_ptr) {
    // 1. 线性查找注册表
    for (int i = 0; i < ctx.count; i++) {
        if (ctx.entries[i].func_ptr == func_ptr) {
            // 2. 找到了，打印堆栈
            print_stack_trace(i);
            return;
        }
    }
    // 3. 未注册，直接返回（不追踪）
}
```

### 时间复杂度

| 操作 | 复杂度 | 说明 |
|-----|--------|------|
| 注册 | O(n) | 需要检查是否已存在 |
| 查找 | O(n) | 线性搜索 |
| 取消注册 | O(n) | 需要查找并移动元素 |

对于典型使用场景（< 64 个函数指针），线性搜索足够高效。

---

## 时间戳与统计

### 时间戳获取

```c
struct timespec ts;
clock_gettime(CLOCK_MONOTONIC, &ts);

// CLOCK_MONOTONIC: 单调递增时钟，不受系统时间修改影响
// ts.tv_sec:  秒
// ts.tv_nsec: 纳秒

// 格式化输出：秒.微秒
fprintf(fp, "时间戳: %ld.%06ld\n", 
        (long)ts.tv_sec, 
        ts.tv_nsec / 1000);  // 纳秒转微秒
```

### 为什么用 CLOCK_MONOTONIC

| 时钟类型 | 特点 | 适用场景 |
|---------|------|---------|
| CLOCK_REALTIME | 系统时间，可能被修改 | 显示时间 |
| CLOCK_MONOTONIC | 单调递增，不受影响 | 性能测量、时序分析 |

### 统计信息

```c
// 每个函数的调用计数
entries[i].call_count++;

// 总追踪次数
total_traced++;

// cleanup 时输出统计
各函数调用统计:
  ops->read         -> usb_read         : 3 次
  ops->write        -> usb_write        : 1 次
  ops->on_event     -> event_handler    : 2 次
```

---

## 日志输出格式

### 日志结构

```
================================================================================
fptrace 堆栈追踪启动: Tue Feb  3 10:30:00 2026
================================================================================

[注册 #1] ops->read -> usb_read (0x10880)
[注册 #2] ops->write -> usb_write (0x108b4)

┌─────────────────────────────────────────────────────────────────────────────
│ [调用 #1] ops->read -> usb_read (0x10880)
│ 时间戳: 12345.678901  (第 1 次调用此函数)
├─────────────────────────────────────────────────────────────────────────────
│ 调用堆栈 (深度 5):
│   #0  process (0x10abc)
│   #1  dispatch (0x10def)
│   #2  main (0x10123)
│   #3  __libc_start_main (0xb6e12345)
│   #4  _start (0x10000)
└─────────────────────────────────────────────────────────────────────────────

================================================================================
fptrace 堆栈追踪结束: Tue Feb  3 10:30:05 2026
================================================================================
统计信息:
  已注册函数指针: 2 个
  总追踪调用次数: 5 次

各函数调用统计:
  ops->read                      -> usb_read             : 3 次
  ops->write                     -> usb_write            : 2 次
================================================================================
```

### 堆栈层级解读

```
│   #0  process (0x10abc)          ← 直接调用 FPT_TRACE 的函数
│   #1  dispatch (0x10def)         ← 调用 process 的函数
│   #2  main (0x10123)             ← 调用 dispatch 的函数
│   #3  __libc_start_main          ← C 运行时
│   #4  _start                     ← 程序入口点
```

**注意**：`#0` 显示的地址是返回地址，指向 `process` 中 `call do_call` 指令之后的位置，不是 `process` 的入口地址。

### 跳过的层级

```c
// backtrace() 返回的前两层：
// stack[0] = fptrace_stack_check 内部
// stack[1] = FPT_TRACE 宏展开位置

// 从 i=2 开始输出，跳过这两层
for (i = 2; i < depth; i++) {
    fprintf(fp, "│   #%-2d %s (%p)\n", i - 2, ...);
}
```

---

## NO_BACKTRACE 模式

### 为什么需要这个模式

某些嵌入式环境没有 `backtrace()` 函数：
- 非 glibc 的 C 库（如 musl libc 的某些配置）
- 极简化的嵌入式 Linux
- 自定义的 C 运行时

### 编译方式

```bash
gcc -DNO_BACKTRACE your_code.c fptrace.c fptrace_stack.c -o program
```

### 行为差异

```c
#ifndef NO_BACKTRACE
    // 正常模式：获取并打印堆栈
    depth = backtrace(stack, FPTRACE_MAX_STACK_DEPTH);
    symbols = backtrace_symbols(stack, depth);
    // ... 打印堆栈信息
#else
    // NO_BACKTRACE 模式：只打印提示
    fprintf(fp, "│ (堆栈追踪不可用 - NO_BACKTRACE 模式)\n");
#endif
```

### 功能对比

| 功能 | 正常模式 | NO_BACKTRACE 模式 |
|-----|---------|-------------------|
| 函数注册 | ✅ | ✅ |
| 调用检测 | ✅ | ✅ |
| 调用计数 | ✅ | ✅ |
| 时间戳 | ✅ | ✅ |
| 堆栈打印 | ✅ | ❌ |

---

## 常见问题

### 1. 堆栈深度只有 1-2 层

**原因**：编译时启用了帧指针省略优化。

**解决**：
```bash
gcc -fno-omit-frame-pointer -g your_code.c -o program
```

### 2. 函数名显示为 (unknown)

**原因**：
- 程序被 strip 了
- 没有使用 `-rdynamic` 链接

**解决**：
```bash
gcc -g -rdynamic your_code.c -o program
```

### 3. backtrace_symbols 返回 NULL

**原因**：内存分配失败。

**处理**：代码中已包含检查：
```c
if (symbols && strcmp(fname, "(unknown)") == 0) {
    fprintf(fp, "│       └── %s\n", symbols[i]);
}
```

### 4. 想要更精确的行号信息

**方法**：使用 `addr2line` 工具后处理：
```bash
# 从日志中提取地址
grep "0x" trace.log | addr2line -e ./program -f -C
```

或在代码中调用：
```c
char cmd[256];
snprintf(cmd, sizeof(cmd), "addr2line -e /proc/self/exe -f %p", addr);
system(cmd);
```

### 5. 多线程环境的注意事项

当前实现使用全局变量，多线程环境下：
- **注册操作**：建议在主线程初始化完成
- **追踪操作**：每次调用独立，但日志输出可能交错
- **统计数据**：非原子操作，计数可能不精确

如需严格线程安全，可添加互斥锁：
```c
#include <pthread.h>
static pthread_mutex_t stack_mutex = PTHREAD_MUTEX_INITIALIZER;

void fptrace_stack_check(void *func_ptr) {
    pthread_mutex_lock(&stack_mutex);
    // ... 原有逻辑
    pthread_mutex_unlock(&stack_mutex);
}
```

---

## 实现细节

### 完整的 check 流程

```c
void fptrace_stack_check(void *func_ptr) {
    // 1. 检查初始化
    if (!ctx.initialized) return;
    
    // 2. 查找注册表
    int idx = -1;
    for (int i = 0; i < ctx.count; i++) {
        if (ctx.entries[i].func_ptr == func_ptr) {
            idx = i;
            break;
        }
    }
    if (idx < 0) return;  // 未注册
    
    // 3. 更新统计
    ctx.entries[idx].call_count++;
    ctx.total_traced++;
    
    // 4. 获取时间戳
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    
    // 5. 获取堆栈
    void *stack[32];
    int depth = backtrace(stack, 32);
    char **symbols = backtrace_symbols(stack, depth);
    
    // 6. 输出到日志
    fprintf(fp, "┌───...\n");
    fprintf(fp, "│ [调用 #%d] %s -> %s\n", ...);
    fprintf(fp, "│ 时间戳: %ld.%06ld\n", ...);
    for (int i = 2; i < depth; i++) {
        const char *fname = fptrace_name(stack[i]);
        fprintf(fp, "│   #%d %s (%p)\n", i-2, fname, stack[i]);
    }
    fprintf(fp, "└───...\n");
    fflush(fp);
    
    // 7. 清理
    if (symbols) free(symbols);
}
```

### 宏展开

```c
// FPT_TRACE(ops->read, buf, len) 展开为:
do {
    fptrace_stack_check((void *)(ops->read));  // 追踪
    (ops->read)(buf, len);                     // 实际调用
} while(0)

// FPT_TRACE_RET(ops->read, buf, len) 展开为:
({
    fptrace_stack_check((void *)(ops->read));  // 追踪
    (ops->read)(buf, len);                     // 返回值自动传递
})
```

---

## 参考资料

- [man backtrace](https://man7.org/linux/man-pages/man3/backtrace.3.html)
- [man clock_gettime](https://man7.org/linux/man-pages/man2/clock_gettime.2.html)
- [GCC -fomit-frame-pointer](https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html)
- [x86-64 ABI](https://refspecs.linuxbase.org/elf/x86_64-abi-0.99.pdf) - 栈帧布局
- [ARM AAPCS](https://developer.arm.com/documentation/ihi0042/latest/) - ARM 调用约定

