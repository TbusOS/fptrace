# fptrace

**Function Pointer Trace** - 在运行时获取函数指针指向的函数名称，并追踪调用堆栈

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![GitHub](https://img.shields.io/github/stars/TbusOS/fptrace?style=social)](https://github.com/TbusOS/fptrace)

## 解决什么问题？

在大型项目中，经常遇到这样的代码：

```c
struct device_ops {
    int (*read)(void *buf, int len);
    int (*write)(void *buf, int len);
    void (*on_event)(int type);
};

// 某处调用
ops->read(buf, len);  // 这个 read 到底指向哪个函数？？？
```

**代码量大时，根本不知道这个函数指针实际指向了谁！**

使用 fptrace，可以：

```c
#include "fptrace.h"

// 1. 打印函数名
printf("read -> %s\n", fptrace_name((void *)ops->read));
// 输出: read -> usb_device_read
```

还可以追踪调用堆栈：

```c
#include "fptrace_stack.h"

// 2. 注册并追踪调用堆栈
fptrace_stack_init("trace.log");
FPT_REGISTER(ops->read);
FPT_TRACE(ops->read, buf, len);  // 自动记录调用堆栈到日志
```

## 特性

- **纯代码实现** - 不依赖 addr2line、atos 等外部命令
- **嵌入式友好** - 支持嵌入式 Linux 环境（ARM32/ARM64）
- **线程安全** - 提供多种线程安全版本
- **零配置** - 无需预注册函数，自动解析符号表
- **模块化设计** - 函数名解析和堆栈追踪分离，按需使用
- **调用堆栈追踪** - 记录完整调用链，支持日志输出

## 目录结构

```
fptrace/
├── README.md                    # 使用指南
├── TECHNICAL.md                 # 函数名解析原理文档
├── TECHNICAL_STACK.md           # 堆栈追踪原理文档
├── Makefile
├── src/                         # 📦 库源码
│   ├── fptrace.h                # 函数名解析 API
│   ├── fptrace.c                # 函数名解析实现
│   ├── fptrace_stack.h          # 堆栈追踪 API
│   └── fptrace_stack.c          # 堆栈追踪实现
└── examples/                    # 📝 示例代码
    ├── test_fptrace.c           # 基础示例
    ├── test_stack_trace.c       # 堆栈追踪示例
    └── test_multi_handler.c     # 多 handler 追踪示例
```

## 快速开始

### 1. 复制 src 目录到你的项目

```bash
# 只需函数名解析
cp src/fptrace.h src/fptrace.c your_project/

# 需要堆栈追踪（同时复制）
cp src/fptrace*.h src/fptrace*.c your_project/
```

### 2. 函数名解析（基础功能）

```c
#include "fptrace.h"

// 获取函数名
printf("回调: %s\n", fptrace_name((void *)callback));

// 获取带地址的格式
printf("%s\n", fptrace_fmt((void *)callback));
// 输出: some_handler (0x12345678)

// 用宏快速打印
FPT_PRINT(callback);
// 输出: callback = some_handler (0x12345678)

// 调用前自动打印
FPT_CALL(ops->read, buf, len);
// 输出: [fptrace] ops->read -> usb_read (0x...)
// 然后自动调用 ops->read(buf, len)
```

### 3. 调用堆栈追踪（高级功能）

```c
#include "fptrace_stack.h"  // 已包含 fptrace.h

// 1. 初始化，指定日志文件
fptrace_stack_init("trace.log");

// 2. 注册要追踪的函数指针
FPT_REGISTER(ops->read);
FPT_REGISTER(ops->write);

// 3. 调用时自动记录堆栈
FPT_TRACE(ops->read, buf, len);

// 4. 清理
fptrace_stack_cleanup();
```

日志输出示例：
```
┌─────────────────────────────────────────────────────────────────────────────
│ [调用 #1] ops->read -> usb_read (0x10880)
│ 时间戳: 12345.678901  (第 1 次调用此函数)
├─────────────────────────────────────────────────────────────────────────────
│ 调用堆栈 (深度 5):
│   #0  process_request (0x10abc)
│   #1  handle_command (0x10def)
│   #2  main (0x10123)
└─────────────────────────────────────────────────────────────────────────────
```

### 4. 编译

```bash
# 只用函数名解析
gcc -g your_code.c fptrace.c -ldl -rdynamic -o your_program

# 使用堆栈追踪
gcc -g your_code.c fptrace.c fptrace_stack.c -ldl -rdynamic -o your_program
```

## API 参考

### 函数名解析 API（fptrace.h）

| 函数 | 说明 | 线程安全 |
|-----|------|---------|
| `fptrace_name(ptr)` | 获取函数名 | ✅ |
| `fptrace_fmt(ptr)` | 获取 `"name (addr)"` 格式 | ❌ |
| `fptrace_print(ptr)` | 打印详细信息 | ✅ |
| `fptrace_name_r(ptr, buf, size)` | 线程安全版本 | ✅ |
| `fptrace_fmt_r(ptr, buf, size)` | 线程安全格式化 | ✅ |
| `fptrace_fmt_tls(ptr)` | TLS 线程安全版本 | ✅ |
| `fptrace_debug()` | 打印诊断信息 | ✅ |

| 宏 | 说明 |
|----|------|
| `FPT_PRINT(ptr)` | 打印 `ptr = name (addr)` |
| `FPT_CALL(ptr, args...)` | 打印后调用 |
| `FPT_CALL_RET(ptr, args...)` | 带返回值版本 |

### 堆栈追踪 API（fptrace_stack.h）

| 函数 | 说明 |
|-----|------|
| `fptrace_stack_init(log_file)` | 初始化，NULL 输出到 stderr |
| `fptrace_stack_register(ptr, name)` | 注册函数指针 |
| `fptrace_stack_unregister(ptr)` | 取消注册 |
| `fptrace_stack_check(ptr)` | 检查并打印堆栈 |
| `fptrace_stack_stats(&reg, &traced)` | 获取统计信息 |
| `fptrace_stack_cleanup()` | 清理资源 |

| 宏 | 说明 |
|----|------|
| `FPT_REGISTER(ptr)` | 简化注册 |
| `FPT_TRACE(ptr, args...)` | 追踪后调用 |
| `FPT_TRACE_RET(ptr, args...)` | 带返回值版本 |

## 示例

### 调试结构体中的函数指针

```c
struct file_ops *ops = get_file_ops();

// 打印所有函数指针
FPT_PRINT(ops->open);    // ops->open = local_file_open (0x401234)
FPT_PRINT(ops->read);    // ops->read = local_file_read (0x401280)
FPT_PRINT(ops->close);   // ops->close = local_file_close (0x4012c0)
```

### 追踪多个 Handler 的调用堆栈

```c
#include "fptrace_stack.h"

typedef void (*handler_t)(int);
handler_t h1 = handler_A;
handler_t h2 = handler_B;

fptrace_stack_init("trace.log");

// 注册多个 handler
FPT_REGISTER(h1);
FPT_REGISTER(h2);

// 调用时自动追踪
FPT_TRACE(h1, 100);  // 记录 handler_A 的调用堆栈
FPT_TRACE(h2, 200);  // 记录 handler_B 的调用堆栈

fptrace_stack_cleanup();  // 输出统计信息
```

### 多线程环境

```c
void *worker_thread(void *arg) {
    callback_t cb = get_callback();
    
    // 使用 TLS 版本，自动线程安全
    printf("Thread: %s\n", fptrace_fmt_tls((void *)cb));
    
    // 或使用用户缓冲区
    char buf[128];
    fptrace_fmt_r((void *)cb, buf, sizeof(buf));
}
```

## 编译运行示例

```bash
make              # 编译基础示例
make examples     # 编译所有示例（包括堆栈追踪）
make run          # 运行基础示例
make run-stack    # 运行堆栈追踪示例
make run-multi    # 运行多 handler 示例
make clean        # 清理
```

## 编译选项

### 基础选项

```bash
-ldl         # 必须：链接动态链接库
-rdynamic    # 推荐：导出符号到动态符号表
-g           # 建议：保留调试信息
```

### 编译模式

| 模式 | 命令 | 说明 |
|-----|------|------|
| 默认 (dladdr) | `make` | 需要 libdl，可解析所有函数 |
| NO_DLADDR | `make NO_DLADDR=1` | 不需要 libdl，只解析主程序函数 |
| NO_BACKTRACE | `make NO_BACKTRACE=1` | 禁用堆栈追踪 |

### 交叉编译

```bash
make CC=arm-none-linux-gnueabi-gcc
make CC=aarch64-linux-gnu-gcc
```

### 模式对比

| 特性 | dladdr (默认) | NO_DLADDR |
|-----|--------------|-----------|
| 依赖 | libdl | 无 |
| 解析主程序函数 | ✅ | ✅ |
| 解析共享库函数 | ✅ | ❌ |
| 堆栈追踪 | ✅ | ✅ (需要 backtrace) |
| 链接选项 | `-ldl -rdynamic` | 无 |

## 符号可见性

| 函数类型 | 能否解析 |
|---------|---------|
| 普通全局函数 | ✅ |
| static 函数 | ✅ (需要 `-g`) |
| 库函数 | ✅ (dladdr 模式) |
| strip 后 | ❌ |
| 内联函数 | ❌ |

## 嵌入式环境（NO_DLADDR 模式）

如果没有 `libdl`，可以使用纯 ELF 解析模式：

```bash
gcc -DNO_DLADDR main.c fptrace.c -o program
```

**使用条件（必须全部满足）：**

| 条件 | 说明 |
|-----|------|
| Linux 系统 | 依赖 /proc 文件系统和 ELF 格式 |
| `/proc/self/exe` 可读 | 用于获取可执行文件路径 |
| `/proc/self/maps` 可读 | 用于获取 PIE/ASLR 加载基地址 |
| 可执行文件存在且可读 | 需要 mmap 文件来解析 ELF 符号表 |
| 可执行文件未被 strip | 需要 .symtab 或 .dynsym 符号表 |

## 技术原理

### 函数名解析原理

详见 [TECHNICAL.md](TECHNICAL.md)

- ELF 符号表解析
- dladdr() 工作原理
- PIE/ASLR 处理
- 手动解析 ELF（NO_DLADDR 模式）

### 堆栈追踪原理

详见 [TECHNICAL_STACK.md](TECHNICAL_STACK.md)

- backtrace() 工作机制
- 栈帧结构（x86-64 / ARM32）
- 注册表机制
- 时间戳与统计

## License

MIT License
