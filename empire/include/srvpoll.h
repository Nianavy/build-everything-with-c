#ifndef SRVPOLL_H
#define SRVPOLL_H

#include <unistd.h>     // 用于 close, ssize_t
#include <sys/socket.h> // 用于 socket, accept, send, recv
#include <netinet/in.h> // 用于 sockaddr_in, htons, htonl
#include <arpa/inet.h>  // 用于 inet_ntoa, inet_pton
#include <poll.h>       // 用于 poll, pollfd
#include <string.h>     // 用于 memset
#include "common.h"     // 包含通用宏和协议结构
#include "parse.h"      // 包含数据库解析相关结构

/**
 * @brief 服务器支持的最大客户端连接数
 */
#define MAX_CLIENTS 256
/**
 * @brief 服务器监听的默认端口号
 */
#define SERVER_PORT 3333

/**
 * @brief 客户端连接的有限状态机 (FSM) 状态
 */
typedef enum {
    STATE_NEW,             ///< 客户端槽位是新的，未被使用
    STATE_CONNECTED,       ///< 客户端已连接到服务器，等待 Hello 请求
    STATE_HELLO_SENT,      ///< (客户端侧状态) 客户端已发送 Hello，等待服务器响应
    STATE_AUTH_PENDING,    ///< Hello 验证正在进行中 (服务器侧状态)
    STATE_READY_FOR_MSG,   ///< 客户端已通过 Hello 验证，可以发送业务消息
    STATE_DISCONNECTED,    ///< 客户端已断开连接
    STATE_ERROR            ///< 客户端进入错误状态，通常会断开
} client_state_e;

/**
 * @brief 存储每个客户端连接的状态信息
 */
typedef struct {
    int fd;                 ///< 客户端的套接字文件描述符，-1 表示空闲
    client_state_e state;   ///< 客户端的当前状态
    char buffer[CLIENT_BUFFER_SIZE]; ///< 用于接收客户端数据的缓冲区
    size_t buffer_pos;              ///< 当前缓冲区已接收数据的末尾位置
    size_t msg_expected_len;        ///< 当前正在接收的消息，其预期的总长度 (头部 + 消息体)
} clientstate_t;

/**
 * @brief 初始化所有客户端状态槽位
 * @param clientStates 客户端状态数组
 * @param max_clients 数组的最大大小
 */
void init_clients(clientstate_t *clientStates, int max_clients);

/**
 * @brief 查找一个空闲的客户端状态槽位
 * @param clientStates 客户端状态数组
 * @param max_clients 数组的最大大小
 * @return 空闲槽位的索引，或 STATUS_ERROR (-1) 如果没有空闲槽位。
 */
int find_free_slot(clientstate_t *clientStates, int max_clients);

/**
 * @brief 根据文件描述符查找对应的客户端状态槽位
 * @param clientStates 客户端状态数组
 * @param max_clients 数组的最大大小
 * @param fd 要查找的文件描述符
 * @return 槽位的索引，或 STATUS_ERROR (-1) 如果未找到。
 */
int find_slot_by_fd(clientstate_t *clientStates, int max_clients, const int fd);

/**
 * @brief 处理单个客户端连接的有限状态机逻辑。
 *        根据客户端的当前状态和接收到的消息进行处理和响应。
 * @param dbhdr 指向数据库头部，用于操作数据
 * @param employees 指向员工数组的指针，FSM 可能会修改它（例如添加/删除员工）
 * @param client 指向当前要处理的客户端状态
 */
void handle_client_fsm(struct dbheader_t *dbhdr, struct employee_t **employees, clientstate_t *client);

/**
 * @brief 封装关闭客户端连接的逻辑。
 * @param client 指向要关闭的客户端状态
 * @param nfds_ptr 指向 poll 描述符数量的指针（在 poll_loop 内部使用）
 */
void close_client_connection(clientstate_t *client, int *nfds_ptr);

#endif