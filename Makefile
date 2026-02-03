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
#
# 模式3: 禁用 backtrace（某些嵌入式环境没有）
#   make NO_BACKTRACE=1
#   - 堆栈追踪功能将不可用
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

ifdef NO_BACKTRACE
    CFLAGS += -DNO_BACKTRACE
    MODE_DESC := $(MODE_DESC) + NO_BACKTRACE
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
EXAMPLE_STACK = $(BUILD_DIR)/test_stack_trace

.PHONY: all clean run run-stack help examples

all: $(BUILD_DIR) $(EXAMPLE)
	@echo ""
	@echo "编译完成! 模式: $(MODE_DESC)"
	@echo "输出: $(EXAMPLE)"
	@echo ""
	@echo "提示: 使用 'make examples' 编译所有示例"

examples: $(BUILD_DIR) $(EXAMPLE) $(EXAMPLE_STACK)
	@echo ""
	@echo "编译完成! 模式: $(MODE_DESC)"
	@echo "输出:"
	@echo "  $(EXAMPLE)"
	@echo "  $(EXAMPLE_STACK)"

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# 编译库对象文件
$(BUILD_DIR)/fptrace.o: $(SRC_DIR)/fptrace.c $(SRC_DIR)/fptrace.h
	$(CC) $(CFLAGS) -c -o $@ $<

# 编译测试示例
$(EXAMPLE): $(EXAMPLE_DIR)/test_fptrace.c $(LIB_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# 编译堆栈追踪示例
$(EXAMPLE_STACK): $(EXAMPLE_DIR)/test_stack_trace.c $(LIB_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# 运行测试
run: $(EXAMPLE)
	./$(EXAMPLE)

# 运行堆栈追踪示例
run-stack: $(EXAMPLE_STACK)
	./$(EXAMPLE_STACK)
	@echo ""
	@echo "=== 日志文件内容 ==="
	@cat trace.log

clean:
	rm -rf $(BUILD_DIR)

help:
	@echo "fptrace - Function Pointer Trace"
	@echo ""
	@echo "编译命令:"
	@echo "  make              - 编译基础示例"
	@echo "  make examples     - 编译所有示例（包括堆栈追踪）"
	@echo ""
	@echo "编译选项:"
	@echo "  make NO_DLADDR=1    - 使用 ELF 手动解析模式（不需要 libdl）"
	@echo "  make NO_BACKTRACE=1 - 禁用堆栈追踪（某些嵌入式环境）"
	@echo ""
	@echo "运行命令:"
	@echo "  make run          - 运行基础示例"
	@echo "  make run-stack    - 运行堆栈追踪示例"
	@echo "  make clean        - 清理"
	@echo ""
	@echo "模式对比:"
	@echo "  +------------------+------------------+--------------------+"
	@echo "  |                  | dladdr (默认)    | NO_DLADDR          |"
	@echo "  +------------------+------------------+--------------------+"
	@echo "  | 依赖             | libdl            | 无                 |"
	@echo "  | 解析主程序函数   | 是               | 是                 |"
	@echo "  | 解析共享库函数   | 是               | 否                 |"
	@echo "  | 堆栈追踪         | 是               | 是(需要backtrace)  |"
	@echo "  | 链接选项         | -ldl -rdynamic   | 无                 |"
	@echo "  +------------------+------------------+--------------------+"
	@echo ""
	@echo "示例组合:"
	@echo "  make CC=arm-gcc                      # ARM 交叉编译"
	@echo "  make NO_DLADDR=1 NO_BACKTRACE=1      # 极简嵌入式"
	@echo ""
	@echo "目录结构:"
	@echo "  src/          - 库源码（复制到你的项目使用）"
	@echo "  examples/     - 示例代码"
	@echo "  build/        - 编译输出"
