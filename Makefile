# fptrace - Function Pointer Trace
# https://github.com/TbusOS/fptrace

CC = arm-none-linux-gnueabi-gcc
CFLAGS_BASE = -Wall -Wextra -g -I./src

#=============================================================================
# 编译模式选择（二选一）
#
# 模式1: 使用 dladdr（默认，推荐）
#   make
#   - 需要 libdl
#   - 能解析主程序 + 共享库函数
#
# 模式2: 使用 NO_DLADDR
#   make NO_DLADDR=1
#   - 不需要 libdl
#   - 只能解析主程序函数
#=============================================================================

ifdef NO_DLADDR
    # NO_DLADDR 模式：手动解析 ELF，不需要 libdl
    CFLAGS = $(CFLAGS_BASE) -DNO_DLADDR
    LDFLAGS =
    MODE_DESC = NO_DLADDR (ELF手动解析)
else
    # 默认模式：使用 dladdr，需要 -ldl -rdynamic
    CFLAGS = $(CFLAGS_BASE)
    LDFLAGS = -ldl -rdynamic
    MODE_DESC = dladdr (默认)
endif

# 目录
SRC_DIR = src
EXAMPLE_DIR = examples
BUILD_DIR = build

# 库源文件
LIB_SRC = $(SRC_DIR)/fptrace.c
LIB_OBJ = $(BUILD_DIR)/fptrace.o

# 示例程序
EXAMPLE = $(BUILD_DIR)/test_fptrace

.PHONY: all clean run help

all: $(BUILD_DIR) $(EXAMPLE)
	@echo ""
	@echo "编译完成! 模式: $(MODE_DESC)"
	@echo "输出: $(EXAMPLE)"

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# 编译库对象文件
$(BUILD_DIR)/fptrace.o: $(SRC_DIR)/fptrace.c $(SRC_DIR)/fptrace.h
	$(CC) $(CFLAGS) -c -o $@ $<

# 编译测试示例
$(EXAMPLE): $(EXAMPLE_DIR)/test_fptrace.c $(LIB_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# 运行测试
run: $(EXAMPLE)
	./$(EXAMPLE)

clean:
	rm -rf $(BUILD_DIR)

help:
	@echo "fptrace - Function Pointer Trace"
	@echo ""
	@echo "编译模式:"
	@echo "  make              - 使用 dladdr 模式（默认，需要 libdl）"
	@echo "  make NO_DLADDR=1  - 使用 ELF 手动解析模式（不需要 libdl）"
	@echo ""
	@echo "其他命令:"
	@echo "  make run          - 运行测试"
	@echo "  make clean        - 清理"
	@echo "  make help         - 显示此帮助"
	@echo ""
	@echo "模式对比:"
	@echo "  +------------------+------------------+--------------------+"
	@echo "  |                  | dladdr (默认)    | NO_DLADDR          |"
	@echo "  +------------------+------------------+--------------------+"
	@echo "  | 依赖             | libdl            | 无                 |"
	@echo "  | 解析主程序函数   | 是               | 是                 |"
	@echo "  | 解析共享库函数   | 是               | 否                 |"
	@echo "  | 链接选项         | -ldl -rdynamic   | 无                 |"
	@echo "  +------------------+------------------+--------------------+"
	@echo ""
	@echo "目录结构:"
	@echo "  src/          - 库源码（复制到你的项目使用）"
	@echo "  examples/     - 示例代码"
	@echo "  build/        - 编译输出"
