#include "../../include/srvpoll.h" // 包含 srvpoll.h 声明
#include "../../include/common.h"   // 包含通用宏、协议结构和网络读写函数
#include <stdio.h>    // For perror, fprintf
#include <stdlib.h>   // For exit
#include <arpa/inet.h> // For htonl, ntohl
#include <string.h>   // For memset, memcpy
#include <errno.h>    // For errno, EINTR
#include <sys/socket.h> // For send, recv (虽然 common.h 间接包含，但明确列出是好习惯)


/**
 * @brief 阻塞式发送函数，确保完整发送所有数据。
 *        处理短写 (partial write) 和中断 (EINTR)。
 * @param fd 文件描述符（套接字）
 * @param buf 要发送的数据缓冲区
 * @param len 要发送的字节数
 * @return 成功发送的字节数（等于 len）或 STATUS_ERROR。
 */
ssize_t send_full(int fd, const void *buf, size_t len) {
    size_t total_sent = 0;
    const char *ptr = (const char *)buf; // 字节指针
    while (total_sent < len) {
        ssize_t bytes_sent = send(fd, ptr + total_sent, len - total_sent, 0);
        if (bytes_sent == -1) {
            if (errno == EINTR) continue; // 被信号中断，重试发送
            perror("send");               // 其他错误是实际错误
            return STATUS_ERROR;
        }
        if (bytes_sent == 0) { // 连接关闭
            fprintf(stderr, "Error: send_full connection closed unexpectedly.\n");
            return STATUS_ERROR;
        }
        total_sent += bytes_sent; // 更新已发送字节数
    }
    return total_sent;
}

/**
 * @brief 阻塞式读取函数，确保完整接收所有数据。
 *        处理短读 (partial read) 和中断 (EINTR)。
 * @param fd 文件描述符（套接字）
 * @param buf 接收数据的缓冲区
 * @param len 要接收的字节数
 * @return 成功接收的字节数（等于 len）或 STATUS_ERROR。
 */
ssize_t read_full(int fd, void *buf, size_t len) {
    size_t total_read = 0;
    char *ptr = (char *)buf; // 字节指针
    while (total_read < len) {
        ssize_t bytes_read = recv(fd, ptr + total_read, len - total_read, 0);
        if (bytes_read == -1) {
            if (errno == EINTR) continue; // 被信号中断，重试读取
            perror("recv");               // 其他错误是实际错误
            return STATUS_ERROR;
        }
        if (bytes_read == 0) { // 连接关闭
            fprintf(stderr, "Error: read_full connection closed prematurely. Read %zu of %zu bytes.\n", total_read, len);
            return STATUS_ERROR;
        }
        total_read += bytes_read; // 更新已接收字节数
    }
    return total_read;
}

/**
 * @brief 封装关闭客户端连接的逻辑。
 *        关闭文件描述符，重置客户端状态，并清理缓冲区信息。
 * @param client 指向要关闭的客户端状态。
 * @param nfds_ptr 指向 poll 描述符数量的指针（当前未使用，但保留签名）。
 */
void close_client_connection(clientstate_t *client, int *nfds_ptr) {
    if (client->fd != -1) {
        printf("Closing connection for fd %d\n", client->fd);
        close(client->fd); // 关闭套接字
        client->fd = -1;    // 标记槽位空闲
        client->state = STATE_DISCONNECTED; // 设为断开状态
        client->buffer_pos = 0;             // 清理缓冲区位置
        client->msg_expected_len = 0;       // 清理消息预期长度
    }
}

/**
 * @brief FSM (有限状态机) 响应客户端的 Hello 请求。
 *        发送 Hello 响应，并将客户端状态转换为 READY_FOR_MSG。
 * @param client 指向客户端状态。
 */
static void fsm_reply_hello(clientstate_t *client) {
    char resp_buf[sizeof(dbproto_hdr_t) + sizeof(dbproto_hello_resp)];
    dbproto_hdr_t *hdr = (dbproto_hdr_t *)resp_buf;
    dbproto_hello_resp *hello_resp = (dbproto_hello_resp *)(resp_buf + sizeof(dbproto_hdr_t));

    // 构造 Hello 响应头部
    hdr->type = MSG_HELLO_RESP;
    hdr->len = sizeof(dbproto_hello_resp);
    hello_resp->proto = PROTO_VER; // 服务器协议版本

    // 转换为网络字节序
    hdr->type = htonl(hdr->type);
    hdr->len = htons(hdr->len);
    hello_resp->proto = htons(hello_resp->proto);

    // 发送完整的 Hello 响应消息
    if (send_full(client->fd, resp_buf, sizeof(resp_buf)) == STATUS_ERROR) {
        perror("fsm_reply_hello send_full");
        close_client_connection(client, NULL); // 发送失败则关闭连接
    } else {
        client->state = STATE_READY_FOR_MSG; // 状态转换为就绪
        printf("Client fd %d upgraded to STATE_READY_FOR_MSG\n", client->fd);
    }
}

/**
 * @brief FSM (有限状态机) 响应客户端的错误。
 *        发送通用错误消息，并关闭客户端连接。
 * @param client 指向客户端状态。
 * @param error_type_code 错误消息的类型码 (虽然发送的是 MSG_ERROR)。
 * @param error_msg 详细的错误信息字符串，用于服务器日志。
 */
static void fsm_reply_error(clientstate_t *client, dbproto_type_e error_type_code, const char* error_msg) {
    char resp_buf[sizeof(dbproto_hdr_t)]; // 错误消息体通常为空，只发送头部
    dbproto_hdr_t *hdr = (dbproto_hdr_t *)resp_buf;

    // 构造错误消息头部
    hdr->type = MSG_ERROR;
    hdr->len = 0; // 错误消息体长度为 0

    // 转换为网络字节序
    hdr->type = htonl(hdr->type);
    hdr->len = htons(hdr->len);

    // 发送错误消息
    if (send_full(client->fd, resp_buf, sizeof(resp_buf)) == STATUS_ERROR) {
        perror("fsm_reply_error send_full");
    }
    fprintf(stderr, "Client fd %d sent MSG_ERROR. Reason: %s\n", client->fd, error_msg);
    close_client_connection(client, NULL); // 错误后通常关闭连接
}

/**
 * @brief FSM (有限状态机) 处理添加员工请求。
 *        解析请求中的员工数据，调用 `add_employee`，并发送响应。
 * @param dbhdr 指向数据库头部。
 * @param employees 指向员工数组的指针。
 * @param client 指向客户端状态。
 * @param req_hdr 接收到的请求头部。
 */
static void fsm_handle_add_employee(struct dbheader_t *dbhdr, struct employee_t **employees, clientstate_t *client, dbproto_hdr_t *req_hdr) {
    // 验证消息体长度
    if (req_hdr->len != sizeof(dbproto_employee_add_req_t)) {
        fsm_reply_error(client, MSG_ERROR, "Add employee request length mismatch");
        return;
    }

    // 获取请求体中的员工数据字符串
    dbproto_employee_add_req_t *add_req = (dbproto_employee_add_req_t *)(client->buffer + sizeof(dbproto_hdr_t));
    
    printf("Client fd %d: Received add string: '%s'\n", client->fd, add_req->data);

    // 调用数据库核心函数添加员工
    int status = add_employee(dbhdr, employees, add_req->data);

    char resp_buf[sizeof(dbproto_hdr_t) + sizeof(dbproto_employee_add_resp_t)];
    dbproto_hdr_t *resp_hdr = (dbproto_hdr_t *)resp_buf;
    dbproto_employee_add_resp_t *add_resp = (dbproto_employee_add_resp_t *)(resp_buf + sizeof(dbproto_hdr_t));

    // 构造添加员工响应头部和体
    resp_hdr->type = MSG_EMPLOYEE_ADD_RESP;
    resp_hdr->len = sizeof(dbproto_employee_add_resp_t);
    add_resp->status = (status == STATUS_SUCCESS) ? STATUS_SUCCESS : STATUS_ERROR; // 直接发送 STATUS_SUCCESS/ERROR

    // 转换为网络字节序
    resp_hdr->type = htonl(resp_hdr->type);
    resp_hdr->len = htons(resp_hdr->len);
    add_resp->status = htonl(add_resp->status);

    // 发送完整的添加员工响应消息
    if (send_full(client->fd, resp_buf, sizeof(resp_buf)) == STATUS_ERROR) {
        perror("fsm_handle_add_employee send_full");
        close_client_connection(client, NULL); // 发送失败则关闭连接
    } else {
        printf("Client fd %d: Employee add request processed (status: %d).\n", client->fd, status);
    }
}

/**
 * @brief FSM (有限状态机) 处理列出员工请求。
 *        发送员工总数，然后逐个发送所有员工数据。
 * @param dbhdr 指向数据库头部。
 * @param employees 指向员工数组（只读）。
 * @param client 指向客户端状态。
 * @param req_hdr 接收到的请求头部。
 */
static void fsm_handle_list_employees(struct dbheader_t *dbhdr, const struct employee_t *employees, clientstate_t *client, dbproto_hdr_t *req_hdr) {
    // 列表请求没有消息体，req_hdr->len 应该为 0
    if (req_hdr->len != 0) {
        fsm_reply_error(client, MSG_ERROR, "List employee request has unexpected payload");
        return;
    }

    char resp_buf[sizeof(dbproto_hdr_t) + sizeof(dbproto_employee_list_resp_t)];
    dbproto_hdr_t *resp_hdr = (dbproto_hdr_t *)resp_buf;
    dbproto_employee_list_resp_t *list_resp = (dbproto_employee_list_resp_t *)(resp_buf + sizeof(dbproto_hdr_t));

    // 构造列出员工响应头部和体（包含员工总数）
    resp_hdr->type = MSG_EMPLOYEE_LIST_RESP;
    resp_hdr->len = sizeof(dbproto_employee_list_resp_t);
    list_resp->count = dbhdr->count;

    // 转换为网络字节序
    resp_hdr->type = htonl(resp_hdr->type);
    resp_hdr->len = htons(resp_hdr->len);
    list_resp->count = htons(list_resp->count);

    // 首先发送响应头部和员工数量
    if (send_full(client->fd, resp_buf, sizeof(resp_buf)) == STATUS_ERROR) {
        perror("fsm_handle_list_employees send_full header");
        close_client_connection(client, NULL);
        return;
    }

    // 然后逐个发送员工数据结构
    if (dbhdr->count > 0 && employees == NULL) {
        fprintf(stderr, "Error: dbhdr->count > 0 but employees is NULL in fsm_handle_list_employees.\n");
        fsm_reply_error(client, MSG_ERROR, "Server internal error: Employees data missing");
        return;
    }

    for (uint16_t i = 0; i < dbhdr->count; ++i) {
        struct employee_t temp_employee = employees[i]; // 创建副本进行转换
        temp_employee.hours = htonl(temp_employee.hours); // 转换 hours 字段为网络字节序

        if (send_full(client->fd, &temp_employee, sizeof(struct employee_t)) == STATUS_ERROR) {
            perror("fsm_handle_list_employees send_full employee data");
            close_client_connection(client, NULL); // 发送失败则关闭连接
            return;
        }
    }
    printf("Client fd %d: Employee list sent (%hu records).\n", client->fd, dbhdr->count);
}

/**
 * @brief FSM (有限状态机) 处理删除员工请求。
 *        调用 `remove_employee` 删除最后一个员工，并发送响应。
 * @param dbhdr 指向数据库头部。
 * @param employees 指向员工数组的指针。
 * @param client 指向客户端状态。
 * @param req_hdr 接收到的请求头部。
 */
static void fsm_handle_remove_employee(struct dbheader_t *dbhdr, struct employee_t **employees, clientstate_t *client, dbproto_hdr_t *req_hdr) {
    // 删除请求没有消息体，req_hdr->len 应该为 0
    if (req_hdr->len != 0) {
        fsm_reply_error(client, MSG_ERROR, "Remove employee request has unexpected payload");
        return;
    }

    // 调用数据库核心函数删除员工
    int status = remove_employee(dbhdr, employees);

    char resp_buf[sizeof(dbproto_hdr_t) + sizeof(dbproto_employee_del_resp_t)];
    dbproto_hdr_t *resp_hdr = (dbproto_hdr_t *)resp_buf;
    dbproto_employee_del_resp_t *del_resp = (dbproto_employee_del_resp_t *)(resp_buf + sizeof(dbproto_hdr_t));

    // 构造删除员工响应头部和体
    resp_hdr->type = MSG_EMPLOYEE_DEL_RESP;
    resp_hdr->len = sizeof(dbproto_employee_del_resp_t);
    del_resp->status = (status == STATUS_SUCCESS) ? STATUS_SUCCESS : STATUS_ERROR; // 直接发送 STATUS_SUCCESS/ERROR

    // 转换为网络字节序
    resp_hdr->type = htonl(resp_hdr->type);
    resp_hdr->len = htons(resp_hdr->len);
    del_resp->status = htonl(del_resp->status);

    // 发送完整的删除员工响应消息
    if (send_full(client->fd, resp_buf, sizeof(resp_buf)) == STATUS_ERROR) {
        perror("fsm_handle_remove_employee send_full");
        close_client_connection(client, NULL); // 发送失败则关闭连接
    } else {
        printf("Client fd %d: Employee remove request processed (status: %d).\n", client->fd, status);
    }
}

/**
 * @brief 处理单个客户端连接的有限状态机逻辑。
 *        从客户端缓冲区接收数据，解析消息，并根据客户端状态和消息类型进行处理。
 *        处理短读、连接断开和缓冲区管理。
 * @param dbhdr 指向数据库头部。
 * @param employees 指向员工数组的指针。
 * @param client 指向当前要处理的客户端状态。
 */
void handle_client_fsm(struct dbheader_t *dbhdr, struct employee_t **employees, clientstate_t *client) {
    ssize_t bytes_read;
    dbproto_hdr_t *current_hdr = (dbproto_hdr_t *)client->buffer; // 指向缓冲区中当前消息头部

    // 从套接字接收数据，填充到缓冲区未使用的部分
    bytes_read = recv(client->fd, client->buffer + client->buffer_pos,
                      CLIENT_BUFFER_SIZE - client->buffer_pos, 0);

    if (bytes_read <= 0) {
        // 连接断开或错误
        if (bytes_read == 0) {
            printf("Client fd %d disconnected normally.\n", client->fd);
        } else {
            perror("recv in handle_client_fsm");
        }
        close_client_connection(client, NULL); // 关闭连接
        return;
    }

    client->buffer_pos += bytes_read; // 更新缓冲区中已接收数据的末尾位置

    // 循环处理缓冲区中的完整消息
    while (client->buffer_pos >= sizeof(dbproto_hdr_t)) { // 确保至少收到了头部
        // 如果是首次处理这个消息，解析头部以获取完整消息的预期长度
        if (client->msg_expected_len == 0) {
            dbproto_hdr_t temp_hdr;
            memcpy(&temp_hdr, client->buffer, sizeof(dbproto_hdr_t)); // 临时复制头部进行解析
            temp_hdr.type = ntohl(temp_hdr.type); // 转换为主机字节序
            temp_hdr.len = ntohs(temp_hdr.len);

            // 对消息类型进行基本检查
            if (temp_hdr.type >= MSG_MAX) {
                fprintf(stderr, "Client fd %d: Invalid message type %d. Closing connection.\n", client->fd, temp_hdr.type);
                fsm_reply_error(client, MSG_ERROR, "Invalid message type");
                return;
            }
            // 检查完整消息是否能放入缓冲区
            if (sizeof(dbproto_hdr_t) + temp_hdr.len > CLIENT_BUFFER_SIZE) {
                fprintf(stderr, "Client fd %d: Message length %u exceeds buffer size %d. Closing connection.\n", 
                        client->fd, (unsigned int)(sizeof(dbproto_hdr_t) + temp_hdr.len), CLIENT_BUFFER_SIZE);
                fsm_reply_error(client, MSG_ERROR, "Message too large");
                return;
            }

            client->msg_expected_len = sizeof(dbproto_hdr_t) + temp_hdr.len; // 设置完整消息的预期长度
        }

        // 检查缓冲区是否包含完整的消息
        if (client->buffer_pos >= client->msg_expected_len) {
            // 完整消息已到达，现在可以安全地转换其头部进行处理
            current_hdr->type = ntohl(current_hdr->type);
            current_hdr->len = ntohs(current_hdr->len);

            printf("Client fd %d (state: %d) received message type: %d, len: %d\n", 
                   client->fd, client->state, current_hdr->type, current_hdr->len);

            // 根据客户端状态和消息类型进行分派处理
            switch (client->state) {
                case STATE_CONNECTED: // 客户端刚连接，期望 Hello 请求
                    if (current_hdr->type == MSG_HELLO_REQ) {
                        // 验证 Hello 请求体长度
                        if (current_hdr->len != sizeof(dbproto_hello_req)) {
                            fprintf(stderr, "Client fd %d: Hello request length mismatch. Expected %zu, got %u.\n", 
                                    client->fd, sizeof(dbproto_hello_req), current_hdr->len);
                            fsm_reply_error(client, MSG_ERROR, "Hello request length mismatch");
                            return;
                        }
                        // 获取 Hello 请求体并验证协议版本
                        dbproto_hello_req *hello_req = (dbproto_hello_req *)(client->buffer + sizeof(dbproto_hdr_t));
                        hello_req->proto = ntohs(hello_req->proto);
                        if (hello_req->proto != PROTO_VER) {
                            fprintf(stderr, "Client fd %d: Protocol mismatch. Expected %u, got %u.\n", 
                                    client->fd, PROTO_VER, hello_req->proto);
                            fsm_reply_error(client, MSG_ERROR, "Protocol mismatch");
                            return;
                        }
                        fsm_reply_hello(client); // 发送 Hello 响应
                    } else {
                        fprintf(stderr, "Client fd %d: Expected MSG_HELLO_REQ, got %d. Disconnecting.\n", client->fd, current_hdr->type);
                        fsm_reply_error(client, MSG_ERROR, "Unexpected message type in CONNECTED state");
                        return;
                    }
                    break;

                case STATE_READY_FOR_MSG: // 客户端已就绪，处理业务消息
                    switch (current_hdr->type) {
                        case MSG_EMPLOYEE_ADD_REQ:
                            fsm_handle_add_employee(dbhdr, employees, client, current_hdr);
                            break;
                        case MSG_EMPLOYEE_LIST_REQ:
                            // 传递 *employees (const struct employee_t*) 给处理函数
                            fsm_handle_list_employees(dbhdr, *employees, client, current_hdr);
                            break;
                        case MSG_EMPLOYEE_DEL_REQ:
                            fsm_handle_remove_employee(dbhdr, employees, client, current_hdr);
                            break;
                        default: // 未知消息类型
                            fprintf(stderr, "Client fd %d: Received unknown message type %d in READY state. Disconnecting.\n", client->fd, current_hdr->type);
                            fsm_reply_error(client, MSG_ERROR, "Unknown message type");
                            return;
                    }
                    break;

                default: // 未知或异常客户端状态
                    fprintf(stderr, "Client fd %d: Unknown state %d. Disconnecting.\n", client->fd, client->state);
                    fsm_reply_error(client, MSG_ERROR, "Unknown client state");
                    return;
            }

            // 消息处理完毕，将缓冲区中剩余的未处理数据移到开头
            size_t remaining_bytes = client->buffer_pos - client->msg_expected_len;
            if (remaining_bytes > 0) {
                memmove(client->buffer, client->buffer + client->msg_expected_len, remaining_bytes);
            }
            client->buffer_pos = remaining_bytes;     // 更新缓冲区位置
            client->msg_expected_len = 0;             // 重置，等待下一个消息的头部解析
        } else {
            // 缓冲区中的数据不足以构成完整消息，等待更多数据
            break; 
        }
    }
}

/**
 * @brief 初始化所有客户端状态槽位。
 * @param clientStates 客户端状态数组。
 * @param max_clients 数组的最大大小。
 */
void init_clients(clientstate_t *clientStates, int max_clients) {
    for (int i = 0; i < max_clients; ++i) {
        clientStates[i].fd = -1;                // 文件描述符初始化为 -1，表示空闲
        clientStates[i].state = STATE_NEW;      // 初始状态为 NEW
        clientStates[i].buffer_pos = 0;         // 缓冲区位置清零
        clientStates[i].msg_expected_len = 0;   // 预期消息长度清零
        memset(clientStates[i].buffer, '\0', CLIENT_BUFFER_SIZE); // 清空缓冲区
    }
}

/**
 * @brief 查找一个空闲的客户端状态槽位。
 * @param clientStates 客户端状态数组。
 * @param max_clients 数组的最大大小。
 * @return 空闲槽位的索引，或 STATUS_ERROR (-1) 如果没有空闲槽位。
 */
int find_free_slot(clientstate_t *clientStates, int max_clients) {
    for (int i = 0; i < max_clients; ++i) {
        if (clientStates[i].fd == -1) return i; // 找到 fd 为 -1 的槽位
    }
    return STATUS_ERROR; // 没有空闲槽位
}

/**
 * @brief 根据文件描述符查找对应的客户端状态槽位。
 * @param clientStates 客户端状态数组。
 * @param max_clients 数组的最大大小。
 * @param fd 要查找的文件描述符。
 * @return 槽位的索引，或 STATUS_ERROR (-1) 如果未找到。
 */
int find_slot_by_fd(clientstate_t *clientStates, int max_clients, const int fd) {
    for (int i = 0; i < max_clients; ++i) {
        if (clientStates[i].fd == fd) return i; // 找到 fd 匹配的槽位
    }
    return STATUS_ERROR; // 未找到
}