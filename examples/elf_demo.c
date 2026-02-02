/**
 * ELF 符号表解析原理演示
 * 
 * 展示如何从 ELF 文件中读取符号表并查找函数名
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <elf.h>

/* 适配 32/64 位 */
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

/* 测试用的函数 */
void my_test_func(void) { printf("hello\n"); }
int  another_func(int x) { return x * 2; }

int main(int argc, char *argv[])
{
    const char *exe_path = "/proc/self/exe";  /* Linux: 指向自己的可执行文件 */
    int fd;
    struct stat st;
    void *map;
    
    printf("╔══════════════════════════════════════════╗\n");
    printf("║     ELF 符号表解析原理演示               ║\n");
    printf("╚══════════════════════════════════════════╝\n\n");

    /* macOS 使用实际路径 */
#ifdef __APPLE__
    exe_path = argv[0];
#endif
    (void)argc;

    /*========================================================================
     * 步骤1: 打开并内存映射 ELF 文件
     *========================================================================*/
    printf("【步骤1】打开 ELF 文件: %s\n", exe_path);
    
    fd = open(exe_path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }
    fstat(fd, &st);
    
    map = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    
    if (map == MAP_FAILED) {
        perror("mmap");
        return 1;
    }
    printf("  文件大小: %lld 字节\n", (long long)st.st_size);
    printf("  映射地址: %p\n\n", map);

    /*========================================================================
     * 步骤2: 解析 ELF Header
     *========================================================================*/
    printf("【步骤2】解析 ELF Header\n");
    
    Elf_Ehdr *ehdr = (Elf_Ehdr *)map;
    
    /* 验证 ELF 魔数: 0x7f 'E' 'L' 'F' */
    if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
        printf("  错误: 不是有效的 ELF 文件\n");
        return 1;
    }
    
    printf("  魔数: %02x %c%c%c (ELF标识)\n", 
           ehdr->e_ident[0], ehdr->e_ident[1], 
           ehdr->e_ident[2], ehdr->e_ident[3]);
    printf("  类型: %s\n", ehdr->e_type == ET_EXEC ? "可执行文件" : 
                          ehdr->e_type == ET_DYN ? "共享对象/PIE" : "其他");
    printf("  节头表偏移: 0x%lx\n", (unsigned long)ehdr->e_shoff);
    printf("  节数量: %d\n\n", ehdr->e_shnum);

    /*========================================================================
     * 步骤3: 遍历 Section Headers，找到符号表
     *========================================================================*/
    printf("【步骤3】查找符号表节 (.symtab 或 .dynsym)\n");
    
    Elf_Shdr *shdr = (Elf_Shdr *)((char *)map + ehdr->e_shoff);
    
    /* 节名字符串表 */
    Elf_Shdr *shstrtab = &shdr[ehdr->e_shstrndx];
    const char *section_names = (const char *)map + shstrtab->sh_offset;
    
    Elf_Sym *symtab = NULL;
    const char *strtab = NULL;
    int symcount = 0;
    
    printf("  遍历所有节:\n");
    for (int i = 0; i < ehdr->e_shnum; i++) {
        const char *name = section_names + shdr[i].sh_name;
        
        /* 打印关键的节 */
        if (shdr[i].sh_type == SHT_SYMTAB || 
            shdr[i].sh_type == SHT_DYNSYM ||
            shdr[i].sh_type == SHT_STRTAB) {
            printf("    [%2d] %-20s 类型=%-8s 偏移=0x%08lx 大小=%lu\n",
                   i, name,
                   shdr[i].sh_type == SHT_SYMTAB ? "SYMTAB" :
                   shdr[i].sh_type == SHT_DYNSYM ? "DYNSYM" : "STRTAB",
                   (unsigned long)shdr[i].sh_offset,
                   (unsigned long)shdr[i].sh_size);
        }
        
        /* 优先使用 .symtab（完整符号表），如果没有则用 .dynsym */
        if (shdr[i].sh_type == SHT_SYMTAB) {
            symtab = (Elf_Sym *)((char *)map + shdr[i].sh_offset);
            symcount = shdr[i].sh_size / sizeof(Elf_Sym);
            /* 字符串表由 sh_link 指定 */
            strtab = (const char *)map + shdr[shdr[i].sh_link].sh_offset;
        }
    }
    
    if (!symtab) {
        /* 回退到动态符号表 */
        for (int i = 0; i < ehdr->e_shnum; i++) {
            if (shdr[i].sh_type == SHT_DYNSYM) {
                symtab = (Elf_Sym *)((char *)map + shdr[i].sh_offset);
                symcount = shdr[i].sh_size / sizeof(Elf_Sym);
                strtab = (const char *)map + shdr[shdr[i].sh_link].sh_offset;
                break;
            }
        }
    }
    
    printf("\n  找到符号表: %d 个符号\n\n", symcount);

    /*========================================================================
     * 步骤4: 遍历符号表
     *========================================================================*/
    printf("【步骤4】符号表内容 (只显示函数)\n");
    printf("  %-18s %-8s %s\n", "地址", "大小", "函数名");
    printf("  %s\n", "─────────────────────────────────────────");
    
    int func_count = 0;
    for (int i = 0; i < symcount && func_count < 15; i++) {
        Elf_Sym *sym = &symtab[i];
        unsigned char type = ELF_ST_TYPE(sym->st_info);
        
        /* 只显示函数类型的符号 */
        if (type == STT_FUNC && sym->st_value != 0) {
            const char *name = strtab + sym->st_name;
            printf("  0x%016lx %-8lu %s\n", 
                   (unsigned long)sym->st_value,
                   (unsigned long)sym->st_size,
                   name);
            func_count++;
        }
    }
    printf("  ... (共 %d 个符号)\n\n", symcount);

    /*========================================================================
     * 步骤5: 根据地址查找函数名
     *========================================================================*/
    printf("【步骤5】根据函数指针查找函数名\n");
    
    void *test_ptrs[] = {
        (void *)my_test_func,
        (void *)another_func,
        (void *)main,
    };
    const char *ptr_names[] = {"my_test_func", "another_func", "main"};
    
    for (int t = 0; t < 3; t++) {
        void *target = test_ptrs[t];
        const char *found_name = "(unknown)";
        
        /* 遍历符号表查找匹配的地址 */
        for (int i = 0; i < symcount; i++) {
            Elf_Sym *sym = &symtab[i];
            unsigned char type = ELF_ST_TYPE(sym->st_info);
            
            if (type != STT_FUNC) continue;
            
            /* 检查地址是否匹配（考虑 PIE 偏移） */
            unsigned long sym_addr = sym->st_value;
            unsigned long ptr_addr = (unsigned long)target;
            
            /* PIE 可执行文件需要计算偏移 */
            if (ehdr->e_type == ET_DYN) {
                /* 简化：直接比较低位 */
                if ((ptr_addr & 0xFFFF) == (sym_addr & 0xFFFF)) {
                    found_name = strtab + sym->st_name;
                    break;
                }
            } else {
                if (ptr_addr == sym_addr) {
                    found_name = strtab + sym->st_name;
                    break;
                }
            }
        }
        
        printf("  %s (%p) -> %s\n", ptr_names[t], target, found_name);
    }
    
    printf("\n");

    /*========================================================================
     * 总结
     *========================================================================*/
    printf("【总结】符号表查找原理:\n");
    printf("  1. 打开 ELF 文件 (自己: /proc/self/exe 或 argv[0])\n");
    printf("  2. 解析 ELF Header，找到 Section Header 表\n");
    printf("  3. 在节中找到 .symtab(符号表) 和 .strtab(字符串表)\n");
    printf("  4. 符号表每项: { 地址, 大小, 类型, 名称偏移 }\n");
    printf("  5. 用函数指针地址在符号表中查找，得到名称偏移\n");
    printf("  6. 用名称偏移在字符串表中取出函数名\n");

    munmap(map, st.st_size);
    return 0;
}
