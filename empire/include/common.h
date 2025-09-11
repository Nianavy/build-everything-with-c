#ifndef COMMON_H
#define COMMON_H

#include <errno.h>   // 错误码定义，用于 errno, EINTR
#include <stdint.h>  // 标准整数类型定义，如 uint16_t, uint32_t
#include <stdio.h>   // 标准输入输出库，用于 fprintf, perror 等
#include <stdlib.h>  // 标准库，用于 exit, malloc, free 等
#include <unistd.h>  // POSIX 系统调用，用于 close

/**
 * @brief 操作状态码：错误
 */
#define STATUS_ERROR -1
/**
 * @brief 操作状态码：成功
 */
#define STATUS_SUCCESS 0

/**
 * @brief 协议版本号
 */
#define PROTO_VER 100

/**
 * @brief 客户端/服务器通信缓冲区大小
 */
#define CLIENT_BUFFER_SIZE 4096

/**
 * @brief 用于 GCC cleanup 属性的内联函数：关闭文件描述符
 * @param fd 指向要关闭的文件描述符的指针
 *          当变量作用域结束时（包括函数返回），如果 *fd 不为
 * -1，则关闭文件描述符并将其设为 -1。
 */
static inline void _cleanup_fd_(int *fd) {
    if (*fd != -1) {
        close(*fd);
        *fd = -1;
    }
}

/**
 * @brief 用于 GCC cleanup 属性的内联函数：释放内存指针
 * @param p_ptr 指向要释放的内存指针的指针（void** 经过 void* 转换）
 *          当变量作用域结束时，如果 *ptr 不为 NULL，则释放内存并将其设为 NULL。
 */
static inline void _cleanup_ptr_(void *p_ptr) {
    void **ptr = (void **)p_ptr;  // 强制转换回 void**
    if (*ptr != NULL) {
        free(*ptr);
        *ptr = NULL;
    }
}

/**
 * @brief 数据库协议消息类型枚举
 */
typedef enum {
    MSG_HELLO_REQ,           ///< 客户端发送的 Hello 请求
    MSG_HELLO_RESP,          ///< 服务器发送的 Hello 响应
    MSG_EMPLOYEE_LIST_REQ,   ///< 客户端发送的列出员工请求
    MSG_EMPLOYEE_LIST_RESP,  ///< 服务器发送的列出员工响应
                             ///< (包含数量，后跟员工数据)
    MSG_EMPLOYEE_ADD_REQ,    ///< 客户端发送的添加员工请求
    MSG_EMPLOYEE_ADD_RESP,   ///< 服务器发送的添加员工响应
    MSG_EMPLOYEE_DEL_REQ,    ///< 客户端发送的删除员工请求
    MSG_EMPLOYEE_DEL_RESP,   ///< 服务器发送的删除员工响应
    MSG_ERROR,               ///< 通用错误消息
    MSG_MAX                  ///< 消息类型最大值，用于范围检查
} dbproto_type_e;

/**
 * @brief 数据库协议消息头部结构
 * 所有字段在网络传输时需要进行字节序转换。
 */
typedef struct {
    dbproto_type_e type;  ///< 消息类型
    uint16_t len;  ///< 消息体（payload）的长度，不包括头部，单位字节
} dbproto_hdr_t;

/**
 * @brief Hello 请求的消息体
 */
typedef struct {
    uint16_t proto;  ///< 客户端支持的协议版本
} dbproto_hello_req;

/**
 * @brief Hello 响应的消息体
 */
typedef struct {
    uint16_t proto;  ///< 服务器支持的协议版本
} dbproto_hello_resp;

/**
 * @brief 添加员工请求的消息体中，员工信息字符串的最大长度
 */
#define MAX_EMPLOYEE_ADD_DATA 1024

/**
 * @brief 添加员工请求的消息体结构
 */
typedef struct {
    char data[MAX_EMPLOYEE_ADD_DATA];  ///< 格式为 "name-address-hours" 的字符串
} dbproto_employee_add_req_t;

/**
 * @brief 添加员工响应的消息体结构
 */
typedef struct {
    int status;  ///< 操作结果状态：STATUS_SUCCESS 或 STATUS_ERROR
} dbproto_employee_add_resp_t;

/**
 * @brief 列出员工请求的消息体结构
 * 列表请求通常不需要额外的消息体。
 */
typedef struct {
    // 列表请求没有消息体
} dbproto_employee_list_req_t;

/**
 * @brief 列出员工响应的消息体结构
 * 响应头部后紧跟此结构，之后是 (count) 个 struct employee_t 结构体。
 */
typedef struct {
    uint16_t count;  ///< 数据库中员工的总数
} dbproto_employee_list_resp_t;

/**
 * @brief 删除员工请求的消息体结构
 * 删除请求（删除最后一个）通常不需要额外的消息体。
 */
typedef struct {
    // 删除请求没有消息体
} dbproto_employee_del_req_t;

/**
 * @brief 删除员工响应的消息体结构
 */
typedef struct {
    int status;  ///< 操作结果状态：STATUS_SUCCESS 或 STATUS_ERROR
} dbproto_employee_del_resp_t;

/**
 * @brief 阻塞式发送函数，确保完整发送所有数据
 * @param fd 文件描述符（套接字）
 * @param buf 要发送的数据缓冲区
 * @param len 要发送的字节数
 * @return 成功发送的字节数（等于 len）或 STATUS_ERROR
 */
ssize_t send_full(int fd, const void *buf, size_t len);

/**
 * @brief 阻塞式读取函数，确保完整接收所有数据
 * @param fd 文件描述符（套接字）
 * @param buf 接收数据的缓冲区
 * @param len 要接收的字节数
 * @return 成功接收的字节数（等于 len）或 STATUS_ERROR
 */
ssize_t read_full(int fd, void *buf, size_t len);

#endif