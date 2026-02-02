# fptrace 技术原理

本文档详细介绍 fptrace 库函数指针名称解析的底层实现原理。

## 目录

1. [核心问题](#核心问题)
2. [ELF 文件结构](#elf-文件结构)
3. [符号表详解](#符号表详解)
4. [解析流程](#解析流程)
5. [dladdr() 原理](#dladdr-原理)
6. [手动解析 ELF](#手动解析-elf)
7. [PIE 与 ASLR](#pie-与-aslr)
8. [常见问题](#常见问题)

---

## 核心问题

**问题**：给定一个函数指针（内存地址），如何获取对应的函数名？

**答案**：查询 ELF 文件中的符号表（Symbol Table）。

```
函数指针 0x401234  ──查询符号表──>  "process_data"
```

---

## ELF 文件结构

Linux 可执行文件使用 ELF (Executable and Linkable Format) 格式：

```
┌─────────────────────────────────────────────────────────────┐
│                      ELF 文件布局                            │
├─────────────────────────────────────────────────────────────┤
│  ELF Header (52/64 字节)                                    │
│  ┌─────────────────────────────────────────────────────────┐│
│  │ e_ident[16]   : 魔数 "\x7fELF" + 类型信息               ││
│  │ e_type        : 文件类型 (ET_EXEC/ET_DYN)               ││
│  │ e_entry       : 程序入口点                               ││
│  │ e_phoff       : Program Header 表偏移                   ││
│  │ e_shoff       : Section Header 表偏移  ─────────────┐   ││
│  │ e_shnum       : Section 数量                        │   ││
│  │ e_shstrndx    : 节名字符串表索引                    │   ││
│  └─────────────────────────────────────────────────────│───┘│
│                                                        │    │
├────────────────────────────────────────────────────────│────┤
│  Program Headers (运行时段描述)                        │    │
├────────────────────────────────────────────────────────│────┤
│  .text        (代码段 - 函数的机器码)                  │    │
├────────────────────────────────────────────────────────│────┤
│  .rodata      (只读数据段)                             │    │
├────────────────────────────────────────────────────────│────┤
│  .data        (已初始化数据段)                         │    │
├────────────────────────────────────────────────────────│────┤
│  .bss         (未初始化数据段)                         │    │
├────────────────────────────────────────────────────────│────┤
│  .dynsym      (动态符号表 - 导出的符号)                │    │
├────────────────────────────────────────────────────────│────┤
│  .dynstr      (动态字符串表)                           │    │
├────────────────────────────────────────────────────────│────┤
│  .symtab      (完整符号表 - 所有符号) ◄────────────────│────┤
├────────────────────────────────────────────────────────│────┤
│  .strtab      (字符串表 - 符号名称) ◄──────────────────│────┤
├────────────────────────────────────────────────────────│────┤
│  Section Headers (节描述表) ◄──────────────────────────┘    │
│  ┌─────────────────────────────────────────────────────────┐│
│  │ [0] NULL                                                ││
│  │ [1] .text      offset=0x1000  size=0x5000  type=PROGBITS││
│  │ [2] .rodata    offset=0x6000  size=0x1000  type=PROGBITS││
│  │ [3] .symtab    offset=0x8000  size=0x2000  type=SYMTAB  ││
│  │ [4] .strtab    offset=0xa000  size=0x1000  type=STRTAB  ││
│  │ ...                                                     ││
│  └─────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────┘
```

### 查看命令

```bash
# 查看 ELF Header
readelf -h your_program

# 查看所有节
readelf -S your_program

# 查看符号表
readelf -s your_program
# 或
nm your_program
```

---

## 符号表详解

### 符号表结构

符号表是一个数组，每个元素描述一个符号（函数或变量）：

```c
// 64位 ELF 符号表项 (24 字节)
typedef struct {
    Elf64_Word    st_name;     // 名称在字符串表中的偏移 (4 字节)
    unsigned char st_info;     // 符号类型和绑定信息 (1 字节)
    unsigned char st_other;    // 符号可见性 (1 字节)
    Elf64_Half    st_shndx;    // 所属节索引 (2 字节)
    Elf64_Addr    st_value;    // 符号值/地址 (8 字节) ⭐ 关键字段
    Elf64_Xword   st_size;     // 符号大小 (8 字节)
} Elf64_Sym;
```

### 符号类型 (st_info)

```c
#define STT_NOTYPE  0   // 未知类型
#define STT_OBJECT  1   // 数据对象（变量）
#define STT_FUNC    2   // 函数 ⭐
#define STT_SECTION 3   // 节
#define STT_FILE    4   // 源文件名
```

### 两种符号表

| 符号表 | 说明 | strip 后 |
|--------|------|----------|
| `.symtab` + `.strtab` | 完整符号表，包含所有符号 | 会被移除 |
| `.dynsym` + `.dynstr` | 动态符号表，只有导出符号 | 保留 |

```bash
# 查看完整符号表
readelf -s your_program | grep "\.symtab" -A 100

# 查看动态符号表
readelf --dyn-syms your_program
```

### 示例输出

```
Symbol table '.symtab' contains 150 entries:
   Num:    Value          Size Type    Bind   Vis      Ndx Name
     0: 0000000000000000     0 NOTYPE  LOCAL  DEFAULT  UND 
    45: 0000000000401234    48 FUNC    GLOBAL DEFAULT   14 process_data
    46: 0000000000401280    32 FUNC    GLOBAL DEFAULT   14 handle_event
    47: 0000000000401300    16 FUNC    LOCAL  DEFAULT   14 static_helper
```

- `Value`: 函数地址 (0x401234)
- `Size`: 函数大小 (48 字节)
- `Type`: FUNC 表示函数
- `Bind`: GLOBAL=全局, LOCAL=static
- `Name`: 函数名

---

## 解析流程

### 图解查找过程

```
输入: 函数指针 = 0x401234

步骤1: 定位符号表
┌──────────────────────────────────────────────────────────────┐
│  ELF Header                                                  │
│    e_shoff = 0x5000  (Section Headers 位置)                  │
│    e_shnum = 25      (共 25 个节)                            │
└──────────────────────────────────────────────────────────────┘
                    │
                    ▼
┌──────────────────────────────────────────────────────────────┐
│  Section Headers @ 0x5000                                    │
│    [10] .symtab  offset=0x3000  size=0x900  link=11         │
│    [11] .strtab  offset=0x3900  size=0x400                  │
└──────────────────────────────────────────────────────────────┘
                    │
                    ▼
步骤2: 遍历符号表，查找地址匹配
┌──────────────────────────────────────────────────────────────┐
│  .symtab @ 0x3000 (每项 24 字节，共 60 个符号)               │
│  ┌────────────────────────────────────────────────────────┐ │
│  │ [0]  st_value=0x000000  st_name=0    type=NOTYPE       │ │
│  │ [1]  st_value=0x401000  st_name=1    type=FUNC         │ │
│  │ [2]  st_value=0x401100  st_name=10   type=FUNC         │ │
│  │ [3]  st_value=0x401234  st_name=25   type=FUNC  ◄──────│─│── 匹配!
│  │ [4]  st_value=0x401280  st_name=38   type=FUNC         │ │
│  │ ...                                                    │ │
│  └────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────┘
                    │
                    │ st_name = 25
                    ▼
步骤3: 在字符串表中取出名称
┌──────────────────────────────────────────────────────────────┐
│  .strtab @ 0x3900                                            │
│  ┌────────────────────────────────────────────────────────┐ │
│  │ offset 0:  "\0"                                        │ │
│  │ offset 1:  "main\0"                                    │ │
│  │ offset 10: "init_system\0"                             │ │
│  │ offset 25: "process_data\0"  ◄─────────────────────────│─│── 取出
│  │ offset 38: "handle_event\0"                            │ │
│  │ ...                                                    │ │
│  └────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────┘
                    │
                    ▼
输出: "process_data"
```

### 伪代码

```c
const char *lookup_func_name(void *func_ptr) {
    // 1. 打开 ELF 文件
    void *elf = mmap_file("/proc/self/exe");
    
    // 2. 解析 ELF Header
    Elf_Ehdr *ehdr = (Elf_Ehdr *)elf;
    Elf_Shdr *sections = elf + ehdr->e_shoff;
    
    // 3. 找到 .symtab 和 .strtab
    Elf_Sym *symtab = NULL;
    char *strtab = NULL;
    for (int i = 0; i < ehdr->e_shnum; i++) {
        if (sections[i].sh_type == SHT_SYMTAB) {
            symtab = elf + sections[i].sh_offset;
            strtab = elf + sections[sections[i].sh_link].sh_offset;
            break;
        }
    }
    
    // 4. 遍历符号表，查找匹配的地址
    int sym_count = section_size / sizeof(Elf_Sym);
    for (int i = 0; i < sym_count; i++) {
        if (symtab[i].st_value == (uintptr_t)func_ptr &&
            ELF_ST_TYPE(symtab[i].st_info) == STT_FUNC) {
            // 5. 返回函数名
            return strtab + symtab[i].st_name;
        }
    }
    
    return "(unknown)";
}
```

---

## dladdr() 原理

`dladdr()` 是 glibc 提供的函数，内部实现了上述逻辑：

```c
#include <dlfcn.h>

int dladdr(void *addr, Dl_info *info);

typedef struct {
    const char *dli_fname;  // 所在文件路径
    void       *dli_fbase;  // 文件加载基地址
    const char *dli_sname;  // 最近的符号名称 ⭐
    void       *dli_saddr;  // 符号地址
} Dl_info;
```

### dladdr 内部流程

```c
// dladdr() 伪实现
int dladdr(void *addr, Dl_info *info) {
    // 1. 遍历所有已加载的模块（从链接器的内部数据结构）
    struct link_map *map;
    for (map = _r_debug.r_map; map != NULL; map = map->l_next) {
        // 2. 检查地址是否在该模块范围内
        if (addr >= map->l_addr && addr < map->l_addr + map->size) {
            info->dli_fname = map->l_name;
            info->dli_fbase = (void *)map->l_addr;
            
            // 3. 查找该模块的符号表（动态链接器已加载到内存）
            Elf_Sym *symtab = map->l_info[DT_SYMTAB]->d_un.d_ptr;
            char *strtab = map->l_info[DT_STRTAB]->d_un.d_ptr;
            
            // 4. 遍历符号表查找最近的符号
            Elf_Sym *best = NULL;
            for (每个符号 sym in symtab) {
                if (sym.st_value <= addr && 
                    (best == NULL || sym.st_value > best->st_value)) {
                    best = sym;
                }
            }
            
            if (best) {
                info->dli_sname = strtab + best->st_name;
                info->dli_saddr = (void *)best->st_value;
            }
            return 1;
        }
    }
    return 0;
}
```

### 优势

- **快速**：符号表已被动态链接器加载到内存
- **处理共享库**：可以解析 .so 中的函数
- **处理 PIE/ASLR**：自动计算地址偏移

---

## 手动解析 ELF

当没有 `dladdr()` 时（某些嵌入式环境），需要手动解析：

### 步骤

```c
// 1. 获取可执行文件路径
char exe_path[256];
readlink("/proc/self/exe", exe_path, sizeof(exe_path));

// 2. 内存映射 ELF 文件
int fd = open(exe_path, O_RDONLY);
struct stat st;
fstat(fd, &st);
void *elf = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

// 3. 验证 ELF 魔数
Elf_Ehdr *ehdr = (Elf_Ehdr *)elf;
if (memcmp(ehdr->e_ident, "\x7fELF", 4) != 0) {
    // 不是 ELF 文件
}

// 4. 获取 Section Header 表
Elf_Shdr *shdr = (Elf_Shdr *)((char *)elf + ehdr->e_shoff);

// 5. 遍历找到 .symtab
for (int i = 0; i < ehdr->e_shnum; i++) {
    if (shdr[i].sh_type == SHT_SYMTAB) {
        Elf_Sym *symtab = (Elf_Sym *)((char *)elf + shdr[i].sh_offset);
        char *strtab = (char *)elf + shdr[shdr[i].sh_link].sh_offset;
        int count = shdr[i].sh_size / sizeof(Elf_Sym);
        // 使用 symtab 和 strtab 查找
    }
}
```

---

## PIE 与 ASLR

### 问题

现代 Linux 默认启用 PIE (Position Independent Executable) 和 ASLR (Address Space Layout Randomization)：

```bash
# 检查是否是 PIE
file your_program
# 输出包含 "shared object" 表示是 PIE
```

### 影响

```
ELF 文件中的地址        运行时实际地址
     0x1234          +  0x555555554000 (随机基地址)
        │                    │
        ▼                    ▼
   符号表地址           函数指针值
```

### 解决方案

```c
// 获取加载基地址
void *get_load_base(void) {
    FILE *fp = fopen("/proc/self/maps", "r");
    char line[256];
    void *base = NULL;
    
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "r-xp") && strstr(line, "/your_program")) {
            sscanf(line, "%p", &base);
            break;
        }
    }
    fclose(fp);
    return base;
}

// 查找时减去基地址
unsigned long target = (unsigned long)func_ptr - (unsigned long)load_base;
// 然后用 target 在符号表中查找
```

---

## 常见问题

### 1. 返回 "(unknown)"

**原因**：
- 程序被 `strip` 过
- 函数是内联的
- 地址不在任何符号范围内

**解决**：
```bash
# 检查是否有符号表
nm your_program | wc -l
# 如果输出很少或 "no symbols"，说明被 strip 了

# 重新编译，保留符号
gcc -g your_code.c -o your_program
```

### 2. static 函数无法解析

**原因**：static 函数是本地符号，在 `.dynsym` 中没有。

**解决**：
```bash
# 编译时加 -g，确保 .symtab 存在
gcc -g your_code.c -o your_program
```

### 3. 共享库中的函数

`dladdr()` 可以自动处理，因为动态链接器维护了所有已加载模块的信息。

### 4. 嵌入式环境没有 /proc

需要在编译时将符号表嵌入程序，或使用外部符号文件。

---

## 参考资料

- [ELF Format Specification](https://refspecs.linuxfoundation.org/elf/elf.pdf)
- [man dladdr](https://man7.org/linux/man-pages/man3/dladdr.3.html)
- [man elf](https://man7.org/linux/man-pages/man5/elf.5.html)
- `/usr/include/elf.h` - ELF 数据结构定义
