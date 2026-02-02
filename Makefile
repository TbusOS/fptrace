# fptrace - Function Pointer Trace
# https://github.com/TbusOS/fptrace

CC = gcc
CFLAGS = -Wall -Wextra -g -I./src
LDFLAGS = -ldl

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
	@echo "使用方法:"
	@echo "  make          - 编译"
	@echo "  make run      - 运行测试"
	@echo "  make clean    - 清理"
	@echo ""
	@echo "目录结构:"
	@echo "  src/          - 库源码（复制到你的项目使用）"
	@echo "  examples/     - 示例代码"
	@echo "  build/        - 编译输出"
