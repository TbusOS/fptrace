# fptrace

**Function Pointer Trace** - 在运行时获取函数指针指向的函数名称

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

使用 fptrace，可以在调用前打印出真实的函数名：

```c
#include "fptrace.h"

printf("read -> %s\n", fptrace_name((void *)ops->read));
// 输出: read -> usb_device_read
```

## 特性

- **纯代码实现** - 不依赖 addr2line、atos 等外部命令
- **嵌入式友好** - 支持嵌入式 Linux 环境
- **线程安全** - 提供多种线程安全版本
- **零配置** - 无需预注册函数，自动解析符号表

## 目录结构

```
fptrace/
├── README.md              # 使用指南
├── TECHNICAL.md           # 技术原理文档
├── Makefile
├── src/                   # 📦 库源码（复制这个目录到你的项目）
│   ├── fptrace.h
│   └── fptrace.c
└── examples/              # 📝 示例代码
    └── test_fptrace.c
```

## 快速开始

### 1. 复制 src 目录到你的项目

```bash
cp src/fptrace.h src/fptrace.c your_project/
```

### 2. 在代码中使用

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

// 调用前自动追踪
FPT_CALL(ops->read, buf, len);
// 输出: [fptrace] ops->read -> usb_read (0x...)
// 然后自动调用 ops->read(buf, len)
```

### 3. 编译

```bash
gcc -g your_code.c fptrace.c -ldl -o your_program
```

## API

| 函数 | 说明 | 线程安全 |
|-----|------|---------|
| `fptrace_name(ptr)` | 获取函数名 | ✅ |
| `fptrace_fmt(ptr)` | 获取 `"name (addr)"` 格式 | ❌ |
| `fptrace_print(ptr)` | 打印详细信息 | ✅ |
| `fptrace_name_r(ptr, buf, size)` | 线程安全版本 | ✅ |
| `fptrace_fmt_r(ptr, buf, size)` | 线程安全格式化 | ✅ |
| `fptrace_fmt_tls(ptr)` | TLS 线程安全版本 | ✅ |

| 宏 | 说明 |
|----|------|
| `FPT_PRINT(ptr)` | 打印 `ptr = name (addr)` |
| `FPT_CALL(ptr, args...)` | 追踪后调用 |
| `FPT_CALL_RET(ptr, args...)` | 带返回值版本 |

## 示例

### 调试结构体中的函数指针

```c
struct file_ops *ops = get_file_ops();

// 打印所有函数指针
FPT_PRINT(ops->open);    // ops->open = local_file_open (0x401234)
FPT_PRINT(ops->read);    // ops->read = local_file_read (0x401280)
FPT_PRINT(ops->close);   // ops->close = local_file_close (0x4012c0)
```

### 追踪回调调用

```c
void process_events(event_handler_t handler, int event) {
    // 调用前打印是哪个处理函数
    printf("[EVENT] %s\n", fptrace_fmt((void *)handler));
    handler(event);
}
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
make        # 编译
make run    # 运行测试
make clean  # 清理
```

## 注意事项

### 编译选项

```bash
-ldl    # 必须：链接动态链接库
-g      # 建议：保留调试信息，static 函数也能解析
```

### 符号可见性

| 函数类型 | 能否解析 |
|---------|---------|
| 普通全局函数 | ✅ |
| static 函数 | ✅ (需要 `-g`) |
| 库函数 | ✅ |
| strip 后 | ❌ |
| 内联函数 | ❌ |

### 嵌入式环境（NO_DLADDR 模式）

如果没有 `libdl`，可以使用纯 ELF 解析模式：

```bash
gcc -DNO_DLADDR main.c fptrace.c -o program
```

**NO_DLADDR 模式的使用条件（必须全部满足）：**

| 条件 | 说明 |
|-----|------|
| Linux 系统 | 依赖 /proc 文件系统和 ELF 格式 |
| `/proc/self/exe` 可读 | 用于获取可执行文件路径 |
| `/proc/self/maps` 可读 | 用于获取 PIE/ASLR 加载基地址 |
| 可执行文件存在且可读 | 需要 mmap 文件来解析 ELF 符号表 |
| 可执行文件未被 strip | 需要 .symtab 或 .dynsym 符号表 |

**NO_DLADDR 模式的限制：**
- 只能解析主程序的函数
- 不能解析共享库（.so）中的函数
- 如果需要解析共享库函数，请使用默认的 dladdr 模式

## 技术原理

详见 [TECHNICAL.md](TECHNICAL.md)

- ELF 符号表解析
- dladdr() 工作原理
- PIE/ASLR 处理

## License

MIT License
