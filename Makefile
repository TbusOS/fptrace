# fptrace - Function Pointer Trace
# https://github.com/TbusOS/fptrace

CC = arm-none-linux-gnueabi-gcc
CFLAGS_BASE = -Wall -Wextra -g -I./src

#=============================================================================
# 编译模式选择
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
#
# ARM32 堆栈追踪:
#   make ARM_BACKTRACE=1
#   - 添加 -fno-omit-frame-pointer -mapcs-frame
#   - ARM32 上 backtrace() 需要这些选项才能正常工作
#=============================================================================

# ARM32 特殊处理：backtrace() 需要帧指针和 unwind 信息
ifdef ARM_BACKTRACE
    CFLAGS_BASE += -fno-omit-frame-pointer -mapcs-frame -funwind-tables -fexceptions -O0
    # ARM32 backtrace 可能需要 libgcc_s
    LDFLAGS_ARM = -lgcc_s
endif

ifdef NO_DLADDR
    # NO_DLADDR 模式：手动解析 ELF，不需要 libdl
    CFLAGS = $(CFLAGS_BASE) -DNO_DLADDR
    LDFLAGS = $(LDFLAGS_ARM)
    MODE_DESC = NO_DLADDR (ELF手动解析)
else
    # 默认模式：使用 dladdr，需要 -ldl -rdynamic
    CFLAGS = $(CFLAGS_BASE)
    LDFLAGS = -ldl -rdynamic $(LDFLAGS_ARM)
    MODE_DESC = dladdr (默认)
endif

ifdef NO_BACKTRACE
    CFLAGS += -DNO_BACKTRACE
    MODE_DESC := $(MODE_DESC) + NO_BACKTRACE
endif

ifdef ARM_BACKTRACE
    MODE_DESC := $(MODE_DESC) + ARM_BACKTRACE
endif

# 目录
SRC_DIR = src
EXAMPLE_DIR = examples
BUILD_DIR = build

# 库源文件
LIB_OBJ = $(BUILD_DIR)/fptrace.o
LIB_STACK_OBJ = $(BUILD_DIR)/fptrace_stack.o

# 示例程序
EXAMPLE = $(BUILD_DIR)/test_fptrace
EXAMPLE_STACK = $(BUILD_DIR)/test_stack_trace
EXAMPLE_MULTI = $(BUILD_DIR)/test_multi_handler

.PHONY: all clean run run-stack help examples

all: $(BUILD_DIR) $(EXAMPLE)
	@echo ""
	@echo "编译完成! 模式: $(MODE_DESC)"
	@echo "输出: $(EXAMPLE)"
	@echo ""
	@echo "提示: 使用 'make examples' 编译所有示例"

examples: $(BUILD_DIR) $(EXAMPLE) $(EXAMPLE_STACK) $(EXAMPLE_MULTI)
	@echo ""
	@echo "编译完成! 模式: $(MODE_DESC)"
	@echo "输出:"
	@echo "  $(EXAMPLE)           - 基础函数名解析示例"
	@echo "  $(EXAMPLE_STACK)     - 堆栈追踪示例"
	@echo "  $(EXAMPLE_MULTI)     - 多 handler 追踪示例"

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# 编译 fptrace 库对象文件
$(BUILD_DIR)/fptrace.o: $(SRC_DIR)/fptrace.c $(SRC_DIR)/fptrace.h
	$(CC) $(CFLAGS) -c -o $@ $<

# 编译 fptrace_stack 库对象文件
$(BUILD_DIR)/fptrace_stack.o: $(SRC_DIR)/fptrace_stack.c $(SRC_DIR)/fptrace_stack.h $(SRC_DIR)/fptrace.h
	$(CC) $(CFLAGS) -c -o $@ $<

# 编译基础示例（只需要 fptrace）
$(EXAMPLE): $(EXAMPLE_DIR)/test_fptrace.c $(LIB_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# 编译堆栈追踪示例（需要 fptrace + fptrace_stack）
$(EXAMPLE_STACK): $(EXAMPLE_DIR)/test_stack_trace.c $(LIB_OBJ) $(LIB_STACK_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# 编译多 handler 示例（需要 fptrace + fptrace_stack）
$(EXAMPLE_MULTI): $(EXAMPLE_DIR)/test_multi_handler.c $(LIB_OBJ) $(LIB_STACK_OBJ)
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

# 运行多 handler 示例
run-multi: $(EXAMPLE_MULTI)
	./$(EXAMPLE_MULTI)
	@echo ""
	@echo "=== 日志文件内容 ==="
	@cat multi_trace.log

clean:
	rm -rf $(BUILD_DIR) trace.log multi_trace.log

help:
	@echo "fptrace - Function Pointer Trace"
	@echo ""
	@echo "模块说明:"
	@echo "  fptrace.h/c       - 函数名称解析"
	@echo "  fptrace_stack.h/c - 调用堆栈追踪"
	@echo ""
	@echo "编译命令:"
	@echo "  make              - 编译基础示例"
	@echo "  make examples     - 编译所有示例"
	@echo ""
	@echo "编译选项:"
	@echo "  make NO_DLADDR=1     - 使用 ELF 手动解析模式（不需要 libdl）"
	@echo "  make NO_BACKTRACE=1  - 禁用堆栈追踪（某些嵌入式环境）"
	@echo "  make ARM_BACKTRACE=1 - ARM32 堆栈追踪支持（添加帧指针选项）"
	@echo ""
	@echo "运行命令:"
	@echo "  make run          - 运行基础示例"
	@echo "  make run-stack    - 运行堆栈追踪示例"
	@echo "  make run-multi    - 运行多 handler 示例"
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
	@echo "  make CC=arm-gcc                               # ARM 交叉编译"
	@echo "  make NO_DLADDR=1 ARM_BACKTRACE=1 examples     # ARM32 完整功能"
	@echo "  make NO_DLADDR=1 NO_BACKTRACE=1               # 极简嵌入式"
	@echo ""
	@echo "ARM32 堆栈追踪说明:"
	@echo "  ARM32 上 backtrace() 需要帧指针才能正常工作。"
	@echo "  使用 ARM_BACKTRACE=1 会自动添加以下编译选项:"
	@echo "    -fno-omit-frame-pointer  (保留帧指针)"
	@echo "    -mapcs-frame             (ARM APCS 帧格式)"
	@echo "    -O0                      (禁用优化)"
	@echo ""
	@echo "目录结构:"
	@echo "  src/fptrace.h         - 函数名解析 API"
	@echo "  src/fptrace.c         - 函数名解析实现"
	@echo "  src/fptrace_stack.h   - 堆栈追踪 API"
	@echo "  src/fptrace_stack.c   - 堆栈追踪实现"
	@echo "  examples/             - 示例代码"
	@echo "  build/                - 编译输出"
