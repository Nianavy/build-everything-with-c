# CMakeLists.txt

# 设置项目所需的最低 CMake 版本
cmake_minimum_required(VERSION 3.10) # 建议使用较新版本，提供更多功能

# 定义项目名称
project(DatabaseProject C) # 明确项目是 C 语言项目

# 设置编译选项
# -Wall -Wextra 是好习惯，-g 用于调试，-O2 用于优化
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pthread") # -pthread 放在这里更通用

# 定义头文件目录
# 这是你的 Makefile 中的 -Iinclude，这里直接添加为全局包含目录
include_directories(include)

# 定义输出目录 (CMake 会自动创建)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/lib) # 如果有库，也设置一下
# CMake 默认会将中间对象文件（.o）放在构建目录的 CMakeFiles/<target_name> 目录下
# 如果你真的想把它们放到 obj/srv 和 obj/cli，需要更复杂的配置，通常不推荐这样做
# 因为这会使得构建目录结构混乱。我们遵循 CMake 默认和推荐的做法。
# set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/obj) # 这通常用于静态库的 .a 文件

# --- 服务端相关 ---
# 明确列出服务端源文件，这比 Makefile 的 wildcard 更明确和安全
# 根据你的 `tree` 输出，服务端文件是 main.c, srvpoll.c, parse.c, file.c
set(SRV_SOURCES
    src/srv/main.c
    src/srv/srvpoll.c
    src/srv/parse.c
    src/srv/file.c
)

# 添加服务端可执行文件目标
add_executable(dbserver ${SRV_SOURCES})

# 链接线程库 (现在 CFLAGS 已经包含了 -pthread，这里可以省略，但明确写出更安全)
target_link_libraries(dbserver pthread)

# --- 客户端相关 ---
# 明确列出客户端源文件
# 根据你的 `tree` 输出，客户端文件是 main.c
set(CLI_SOURCES
    src/cli/main.c
)

# 添加客户端可执行文件目标
add_executable(dbcli ${CLI_SOURCES})

# 客户端依赖服务端的部分模块：srvpoll.o, parse.o, file.o
# 为了模拟你的 Makefile 行为，直接添加这些源文件到客户端的链接列表
# CMake 会自动处理这些源文件的编译和链接
target_sources(dbcli PRIVATE
    src/srv/srvpoll.c
    src/srv/parse.c
    src/srv/file.c
)
# 链接线程库 (如果客户端也直接或间接使用 pthread)
target_link_libraries(dbcli pthread)


# --- 清理规则 ---
# CMake 会自动处理构建目录中的清理 ('make clean' 会删除所有 .o 和可执行文件)
# 你可以在这里添加一个自定义的清理目标来删除 .db 文件，
# 因为 CMake 默认不会管理非构建生成的文件
add_custom_target(clean_db COMMAND rm -f *.db WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# 为了方便，提供一个 'clean_all' 目标，清理构建目录和 .db 文件
add_custom_target(clean_all
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR} # 清理 CMake 的构建目录
    COMMAND rm -rf ${CMAKE_SOURCE_DIR}/bin # 清理 bin 目录 (因为我们设置了输出路径)
    COMMAND rm -rf ${CMAKE_SOURCE_DIR}/obj # 清理 obj 目录 (如果它被创建了)
    COMMAND rm -f ${CMAKE_SOURCE_DIR}/*.db # 清理 .db 文件
    COMMENT "Cleaning CMake build artifacts, bin, obj, and .db files"
)

# --- 运行目标 ---
# 自定义的 'run' 目标，执行清理、构建并运行服务端
add_custom_target(run
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR} # 清理构建目录
    COMMAND rm -rf ${CMAKE_SOURCE_DIR}/bin # 清理 bin 目录
    COMMAND rm -rf ${CMAKE_SOURCE_DIR}/obj # 清理 obj 目录
    COMMAND rm -f ${CMAKE_SOURCE_DIR}/*.db # 清理 .db 文件
    COMMAND ${CMAKE_COMMAND} -S ${CMAKE_SOURCE_DIR} -B ${CMAKE_BINARY_DIR} # 重新配置
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} # 重新构建
    COMMAND ${CMAKE_SOURCE_DIR}/bin/dbserver -f test.db -n -p 3333 # 运行服务端
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR} # 运行命令的工作目录 (通常是构建目录)
    COMMENT "Cleaning, building, and running dbserver"
)