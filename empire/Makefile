# 定义编译器和编译选项
CC = gcc
CFLAGS = -Wall
INCLUDE_DIR = -Iinclude

# 定义源文件目录和目标文件目录
SRV_SRC_DIR = src/srv
CLI_SRC_DIR = src/cli
SRV_OBJ_DIR = obj/srv
CLI_OBJ_DIR = obj/cli
BIN_DIR = bin

# 服务端和客户端的源文件列表
SRV_SRCS = $(wildcard $(SRV_SRC_DIR)/*.c)
CLI_SRCS = $(wildcard $(CLI_SRC_DIR)/*.c)

# 对应的目标文件列表 (完整路径)
SRV_OBJS = $(patsubst $(SRV_SRC_DIR)/%.c, $(SRV_OBJ_DIR)/%.o, $(SRV_SRCS))
CLI_OBJS = $(patsubst $(CLI_SRC_DIR)/%.c, $(CLI_OBJ_DIR)/%.o, $(CLI_SRCS))

# 定义可执行文件
TARGET_SRV = $(BIN_DIR)/dbserver
TARGET_CLI = $(BIN_DIR)/dbcli

# 确保 obj 目录存在
$(shell mkdir -p $(SRV_OBJ_DIR) $(CLI_OBJ_DIR) $(BIN_DIR))


.PHONY: all default clean run

all: default

default: $(TARGET_SRV) $(TARGET_CLI)

# 服务端可执行文件
# 依赖所有服务端的目标文件
$(TARGET_SRV): $(SRV_OBJS)
		$(CC) $(CFLAGS) -o $@ $^

# 服务端目标文件编译规则
$(SRV_OBJ_DIR)/%.o: $(SRV_SRC_DIR)/%.c
		$(CC) $(CFLAGS) $(INCLUDE_DIR) -c $< -o $@

# 客户端可执行文件
# 依赖所有客户端的目标文件 AND srvpoll.o (因为 send_full/read_full 在那里实现)
# AND parse.o (因为 add_employee 等函数也在那里实现)
$(TARGET_CLI): $(CLI_OBJS) $(SRV_OBJ_DIR)/srvpoll.o $(SRV_OBJ_DIR)/parse.o
		$(CC) $(CFLAGS) -o $@ $^

# 客户端目标文件编译规则
$(CLI_OBJ_DIR)/%.o: $(CLI_SRC_DIR)/%.c
		$(CC) $(CFLAGS) $(INCLUDE_DIR) -c $< -o $@


clean:
		rm -f $(SRV_OBJ_DIR)/*.o
		rm -f $(CLI_OBJ_DIR)/*.o
		rm -f $(BIN_DIR)/*
		rm -f *.db

run: clean default
		./$(TARGET_SRV) -f test.db -n -p 3333