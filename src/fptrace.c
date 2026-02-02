/**
 * fptrace - Function Pointer Trace
 * 
 * 纯代码实现，不依赖 addr2line、atos 等外部命令
 * 
 * 支持两种模式：
 * 1. 默认模式：使用 dladdr()（大多数 Linux/嵌入式环境都有）
 * 2. 手动模式：定义 NO_DLADDR 宏，手动解析 ELF 符号表
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fptrace.h"

/*============================================================================
 * 方式1：使用 dladdr()（默认，推荐）
 * 大多数嵌入式 Linux 都支持，因为它是 glibc/musl libc 的一部分
 *============================================================================*/
#ifndef NO_DLADDR

#include <dlfcn.h>

const char *fptrace_name(void *func_ptr)
{
    Dl_info info;
    
    if (func_ptr == NULL) {
        return "(null)";
    }
    
    if (dladdr(func_ptr, &info) && info.dli_sname) {
        return info.dli_sname;
    }
    
    return "(unknown)";
}

void fptrace_print(void *func_ptr)
{
    Dl_info info;
    
    if (func_ptr == NULL) {
        printf("Function pointer: NULL\n");
        return;
    }
    
    if (dladdr(func_ptr, &info)) {
        printf("Function: %s\n", info.dli_sname ? info.dli_sname : "(unknown)");
        printf("  Address: %p\n", func_ptr);
        printf("  Module:  %s\n", info.dli_fname ? info.dli_fname : "(unknown)");
        printf("  Base:    %p\n", info.dli_fbase);
    } else {
        printf("Function pointer: %p (cannot resolve)\n", func_ptr);
    }
}

/*============================================================================
 * 方式2：手动解析 ELF 符号表（不依赖 dladdr）
 * 
 * 使用条件（必须全部满足）：
 *   1. Linux 系统（依赖 /proc 文件系统和 ELF 格式）
 *   2. /proc/self/exe 可读（获取可执行文件路径）
 *   3. /proc/self/maps 可读（获取 PIE/ASLR 加载基地址）
 *   4. 可执行文件仍存在且可读（需要 mmap 解析 ELF）
 *   5. 可执行文件未被 strip（需要 .symtab 或 .dynsym 符号表）
 * 
 * 编译方法：
 *   gcc -DNO_DLADDR your_code.c fptrace.c -o program
 * 
 * 适用场景：
 *   - 极简嵌入式 Linux（没有 libdl）
 *   - 静态链接的程序
 *   - 特殊定制的 Linux 环境
 * 
 * 限制：
 *   - 只能解析主程序的函数，不能解析共享库函数
 *   - 依赖 /proc 文件系统
 *============================================================================*/
#else /* NO_DLADDR */

#include <elf.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

/* 64位/32位 ELF 兼容 */
#if __SIZEOF_POINTER__ == 8
    typedef Elf64_Ehdr Elf_Ehdr;
    typedef Elf64_Shdr Elf_Shdr;
    typedef Elf64_Sym  Elf_Sym;
    #define ELF_ST_TYPE ELF64_ST_TYPE
#else
    typedef Elf32_Ehdr Elf_Ehdr;
    typedef Elf32_Shdr Elf_Shdr;
    typedef Elf32_Sym  Elf_Sym;
    #define ELF_ST_TYPE ELF32_ST_TYPE
#endif

/* 缓存的 ELF 信息 */
static struct {
    int          initialized;
    void        *map_base;
    size_t       map_size;
    Elf_Sym     *symtab;
    int          symcount;
    const char  *strtab;
    void        *load_base;  /* 程序加载基地址 */
} elf_cache = {0};

/* 获取程序自身的可执行文件路径 */
static int get_exe_path(char *buf, size_t size)
{
    ssize_t len = readlink("/proc/self/exe", buf, size - 1);
    if (len > 0) {
        buf[len] = '\0';
        return 0;
    }
    return -1;
}

/* 获取程序加载基地址 */
static void *get_load_base(void)
{
    FILE *fp;
    char line[512];
    void *base = NULL;
    char exe_path[256];
    
    if (get_exe_path(exe_path, sizeof(exe_path)) != 0) {
        return NULL;
    }
    
    fp = fopen("/proc/self/maps", "r");
    if (!fp) return NULL;
    
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, exe_path) && strstr(line, "r-x")) {
            sscanf(line, "%p", &base);
            break;
        }
    }
    
    fclose(fp);
    return base;
}

/* 初始化 ELF 解析器 */
static int init_elf_parser(void)
{
    char exe_path[256];
    int fd;
    struct stat st;
    Elf_Ehdr *ehdr;
    Elf_Shdr *shdr;
    int i;
    
    if (elf_cache.initialized) {
        return (elf_cache.symtab != NULL) ? 0 : -1;
    }
    elf_cache.initialized = 1;
    
    /* 获取可执行文件路径 */
    if (get_exe_path(exe_path, sizeof(exe_path)) != 0) {
        return -1;
    }
    
    /* 获取加载基地址 */
    elf_cache.load_base = get_load_base();
    
    /* 打开并映射 ELF 文件 */
    fd = open(exe_path, O_RDONLY);
    if (fd < 0) return -1;
    
    if (fstat(fd, &st) < 0) {
        close(fd);
        return -1;
    }
    
    elf_cache.map_size = st.st_size;
    elf_cache.map_base = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    
    if (elf_cache.map_base == MAP_FAILED) {
        elf_cache.map_base = NULL;
        return -1;
    }
    
    /* 验证 ELF 头 */
    ehdr = (Elf_Ehdr *)elf_cache.map_base;
    if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
        munmap(elf_cache.map_base, elf_cache.map_size);
        elf_cache.map_base = NULL;
        return -1;
    }
    
    /* 查找 .symtab 和 .strtab */
    shdr = (Elf_Shdr *)((char *)elf_cache.map_base + ehdr->e_shoff);
    
    for (i = 0; i < ehdr->e_shnum; i++) {
        if (shdr[i].sh_type == SHT_SYMTAB) {
            elf_cache.symtab = (Elf_Sym *)((char *)elf_cache.map_base + shdr[i].sh_offset);
            elf_cache.symcount = shdr[i].sh_size / sizeof(Elf_Sym);
            /* .strtab 紧跟在 .symtab 后面，由 sh_link 指定 */
            elf_cache.strtab = (const char *)elf_cache.map_base + shdr[shdr[i].sh_link].sh_offset;
            break;
        }
    }
    
    /* 如果没有 .symtab，尝试 .dynsym */
    if (!elf_cache.symtab) {
        for (i = 0; i < ehdr->e_shnum; i++) {
            if (shdr[i].sh_type == SHT_DYNSYM) {
                elf_cache.symtab = (Elf_Sym *)((char *)elf_cache.map_base + shdr[i].sh_offset);
                elf_cache.symcount = shdr[i].sh_size / sizeof(Elf_Sym);
                elf_cache.strtab = (const char *)elf_cache.map_base + shdr[shdr[i].sh_link].sh_offset;
                break;
            }
        }
    }
    
    return (elf_cache.symtab != NULL) ? 0 : -1;
}

/* 根据地址查找符号名 */
static const char *lookup_symbol(void *addr)
{
    int i;
    Elf_Sym *best = NULL;
    unsigned long target = (unsigned long)addr;
    unsigned long best_dist = (unsigned long)-1;
    
    if (init_elf_parser() != 0) {
        return NULL;
    }
    
    /* 如果是 PIE，需要减去加载基地址 */
    if (elf_cache.load_base) {
        target -= (unsigned long)elf_cache.load_base;
    }
    
    for (i = 0; i < elf_cache.symcount; i++) {
        Elf_Sym *sym = &elf_cache.symtab[i];
        unsigned char type = ELF_ST_TYPE(sym->st_info);
        
        /* 只查找函数符号 */
        if (type != STT_FUNC) {
            continue;
        }
        
        /* 检查地址是否在符号范围内 */
        if (target >= sym->st_value && 
            target < sym->st_value + (sym->st_size ? sym->st_size : 0x1000)) {
            unsigned long dist = target - sym->st_value;
            if (dist < best_dist) {
                best_dist = dist;
                best = sym;
            }
        }
    }
    
    if (best && best->st_name) {
        return elf_cache.strtab + best->st_name;
    }
    
    return NULL;
}

const char *fptrace_name(void *func_ptr)
{
    const char *name;
    
    if (func_ptr == NULL) {
        return "(null)";
    }
    
    name = lookup_symbol(func_ptr);
    return name ? name : "(unknown)";
}

void fptrace_print(void *func_ptr)
{
    if (func_ptr == NULL) {
        printf("Function pointer: NULL\n");
        return;
    }
    
    printf("Function: %s\n", fptrace_name(func_ptr));
    printf("  Address: %p\n", func_ptr);
}

#endif /* NO_DLADDR */

/*============================================================================
 * 通用 API（两种模式共用）
 *============================================================================*/

const char *fptrace_fmt(void *func_ptr)
{
    static char buf[256];
    fptrace_fmt_r(func_ptr, buf, sizeof(buf));
    return buf;
}

char *fptrace_name_r(void *func_ptr, char *buf, size_t buf_size)
{
    const char *name;
    
    if (!buf || buf_size == 0) {
        return NULL;
    }
    
    name = fptrace_name(func_ptr);
    strncpy(buf, name, buf_size - 1);
    buf[buf_size - 1] = '\0';
    
    return buf;
}

char *fptrace_fmt_r(void *func_ptr, char *buf, size_t buf_size)
{
    const char *name;
    
    if (!buf || buf_size == 0) {
        return NULL;
    }
    
    if (func_ptr == NULL) {
        snprintf(buf, buf_size, "(null)");
    } else {
        name = fptrace_name(func_ptr);
        snprintf(buf, buf_size, "%s (%p)", name, func_ptr);
    }
    
    return buf;
}

const char *fptrace_fmt_tls(void *func_ptr)
{
    static __thread char tls_buf[256];
    return fptrace_fmt_r(func_ptr, tls_buf, sizeof(tls_buf));
}
