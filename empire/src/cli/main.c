#include <arpa/inet.h>
#include <errno.h>  // For errno
#include <fcntl.h>
#include <getopt.h>  // For getopt
#include <netinet/in.h>
#include <stdbool.h>  // For bool, true, false
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  // For memset, strlen, strncpy
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "../../include/common.h"  // 包含通用宏、协议结构和网络读写函数
#include "../../include/parse.h"  // 包含 struct employee_t 定义

/**
 * @brief 客户端发送 Hello 请求并接收响应，进行协议协商。
 * @param fd 服务器的套接字文件描述符。
 * @return 成功时返回 STATUS_SUCCESS，错误时返回 STATUS_ERROR。
 */
int send_hello(int fd) {
    char buf[CLIENT_BUFFER_SIZE] = {0};  // 缓冲区用于构造和接收消息
    dbproto_hdr_t *hdr = (dbproto_hdr_t *)buf;  // 头部指针
    dbproto_hello_req *hello_req =
        (dbproto_hello_req *)(buf + sizeof(dbproto_hdr_t));  // 请求体指针

    // 构造 Hello 请求头部
    hdr->type = MSG_HELLO_REQ;
    hdr->len = sizeof(dbproto_hello_req);  // 消息体长度

    // 构造 Hello 请求体
    hello_req->proto = PROTO_VER;  // 客户端协议版本

    // 将头部和请求体字段转换为网络字节序
    hdr->type = htonl(hdr->type);
    hdr->len = htons(hdr->len);
    hello_req->proto = htons(hello_req->proto);

    // 发送完整的 Hello 请求消息
    if (send_full(fd, buf, sizeof(dbproto_hdr_t) + sizeof(dbproto_hello_req)) ==
        STATUS_ERROR) {
        perror("send_full hello request");
        return STATUS_ERROR;
    }

    // 接收服务器响应的头部
    if (read_full(fd, buf, sizeof(dbproto_hdr_t)) == STATUS_ERROR) {
        perror("read_full hello response header");
        return STATUS_ERROR;
    }

    // 将接收到的头部字段转换回主机字节序
    hdr->type = ntohl(hdr->type);
    hdr->len = ntohs(hdr->len);

    // 根据响应类型进行处理
    if (hdr->type == MSG_ERROR) {
        printf("Protocol mismatch or server error.\n");
        // 实际应用中可能需要读取错误消息体
        return STATUS_ERROR;
    } else if (hdr->type == MSG_HELLO_RESP) {
        // 验证 Hello 响应体长度
        if (hdr->len != sizeof(dbproto_hello_resp)) {
            fprintf(stderr,
                    "Error: Hello response length mismatch. Expected %zu, got "
                    "%u.\n",
                    sizeof(dbproto_hello_resp), hdr->len);
            return STATUS_ERROR;
        }
        // 接收 Hello 响应体
        dbproto_hello_resp *hello_resp =
            (dbproto_hello_resp *)(buf + sizeof(dbproto_hdr_t));
        if (read_full(fd, hello_resp, sizeof(dbproto_hello_resp)) ==
            STATUS_ERROR) {
            perror("read_full hello response payload");
            return STATUS_ERROR;
        }
        // 转换服务器协议版本为主机字节序
        hello_resp->proto = ntohs(hello_resp->proto);

        // 检查协议版本是否匹配
        if (hello_resp->proto == PROTO_VER) {
            printf("Server connected, protocol v%u.\n", PROTO_VER);
            return STATUS_SUCCESS;
        } else {
            fprintf(stderr, "Protocol mismatch. Server v%u, Client v%u.\n",
                    hello_resp->proto, PROTO_VER);
            return STATUS_ERROR;
        }
    } else {
        fprintf(stderr, "Unexpected message type received: %d\n", hdr->type);
        return STATUS_ERROR;
    }
}

/**
 * @brief 客户端发送添加员工请求并接收响应。
 * @param fd 服务器的套接字文件描述符。
 * @param addstring 格式为 "Name-Address-Hours" 的员工信息字符串。
 * @return 成功时返回 STATUS_SUCCESS，错误时返回 STATUS_ERROR。
 */
int send_add_employee_req(int fd, const char *addstring) {
    char buf[CLIENT_BUFFER_SIZE] = {0};
    dbproto_hdr_t *hdr = (dbproto_hdr_t *)buf;
    dbproto_employee_add_req_t *add_req =
        (dbproto_employee_add_req_t *)(buf + sizeof(dbproto_hdr_t));

    // 检查添加字符串长度是否超出协议限制
    size_t addstring_len = strlen(addstring);
    if (addstring_len >= MAX_EMPLOYEE_ADD_DATA) {
        fprintf(stderr,
                "Error: Employee add string too long (%zu bytes), max is %d.\n",
                addstring_len, MAX_EMPLOYEE_ADD_DATA - 1);
        return STATUS_ERROR;
    }

    // 构造添加员工请求头部
    hdr->type = MSG_EMPLOYEE_ADD_REQ;
    hdr->len = sizeof(dbproto_employee_add_req_t);  // 消息体长度

    // 拷贝员工信息字符串到请求体，并确保空终止
    strncpy(add_req->data, addstring, MAX_EMPLOYEE_ADD_DATA - 1);
    add_req->data[MAX_EMPLOYEE_ADD_DATA - 1] = '\0';

    // 转换为网络字节序
    hdr->type = htonl(hdr->type);
    hdr->len = htons(hdr->len);

    // 发送完整的添加员工请求消息
    if (send_full(fd, buf,
                  sizeof(dbproto_hdr_t) + sizeof(dbproto_employee_add_req_t)) ==
        STATUS_ERROR) {
        perror("send_full add employee request");
        return STATUS_ERROR;
    }

    // 接收服务器响应头部
    if (read_full(fd, buf, sizeof(dbproto_hdr_t)) == STATUS_ERROR) {
        perror("read_full add employee response header");
        return STATUS_ERROR;
    }
    // 转换回主机字节序
    hdr->type = ntohl(hdr->type);
    hdr->len = ntohs(hdr->len);

    // 根据响应类型处理
    if (hdr->type == MSG_ERROR) {
        printf("Server returned an error for add employee.\n");
        return STATUS_ERROR;
    } else if (hdr->type == MSG_EMPLOYEE_ADD_RESP) {
        // 验证响应体长度
        if (hdr->len != sizeof(dbproto_employee_add_resp_t)) {
            fprintf(stderr, "Error: Add employee response length mismatch.\n");
            return STATUS_ERROR;
        }
        // 接收响应体
        dbproto_employee_add_resp_t *add_resp =
            (dbproto_employee_add_resp_t *)(buf + sizeof(dbproto_hdr_t));
        if (read_full(fd, add_resp, sizeof(dbproto_employee_add_resp_t)) ==
            STATUS_ERROR) {
            perror("read_full add employee response payload");
            return STATUS_ERROR;
        }
        // 转换状态码
        add_resp->status = ntohl(add_resp->status);
        if (add_resp->status == STATUS_SUCCESS) {
            printf("Employee added successfully.\n");
            return STATUS_SUCCESS;
        } else {
            fprintf(stderr, "Failed to add employee on server (status: %d).\n",
                    add_resp->status);
            return STATUS_ERROR;
        }
    } else {
        fprintf(stderr,
                "Unexpected message type for add employee response: %d\n",
                hdr->type);
        return STATUS_ERROR;
    }
}

/**
 * @brief 客户端发送列出所有员工的请求，并接收和显示服务器响应的员工列表。
 * @param fd 服务器的套接字文件描述符。
 * @return 成功时返回 STATUS_SUCCESS，错误时返回 STATUS_ERROR。
 */
int send_list_employee_req(int fd) {
    char buf[CLIENT_BUFFER_SIZE] = {0};
    dbproto_hdr_t *hdr = (dbproto_hdr_t *)buf;

    // 构造列出员工请求头部
    hdr->type = MSG_EMPLOYEE_LIST_REQ;
    hdr->len = 0;  // 列表请求没有消息体

    // 转换为网络字节序
    hdr->type = htonl(hdr->type);
    hdr->len = htons(hdr->len);

    // 发送完整的列出员工请求消息
    if (send_full(fd, buf, sizeof(dbproto_hdr_t)) == STATUS_ERROR) {
        perror("send_full list employee request");
        return STATUS_ERROR;
    }

    // 接收服务器响应头部
    if (read_full(fd, buf, sizeof(dbproto_hdr_t)) == STATUS_ERROR) {
        perror("read_full list employee response header");
        return STATUS_ERROR;
    }
    // 转换回主机字节序
    hdr->type = ntohl(hdr->type);
    hdr->len = ntohs(hdr->len);

    // 根据响应类型处理
    if (hdr->type == MSG_ERROR) {
        printf("Server returned an error for list employees.\n");
        return STATUS_ERROR;
    } else if (hdr->type == MSG_EMPLOYEE_LIST_RESP) {
        // 验证响应体长度
        if (hdr->len != sizeof(dbproto_employee_list_resp_t)) {
            fprintf(stderr,
                    "Error: List employee response length mismatch. Expected "
                    "%zu, got %u.\n",
                    sizeof(dbproto_employee_list_resp_t), hdr->len);
            return STATUS_ERROR;
        }
        // 接收响应体（包含员工总数）
        dbproto_employee_list_resp_t *list_resp =
            (dbproto_employee_list_resp_t *)(buf + sizeof(dbproto_hdr_t));
        if (read_full(fd, list_resp, sizeof(dbproto_employee_list_resp_t)) ==
            STATUS_ERROR) {
            perror("read_full list employee response payload");
            return STATUS_ERROR;
        }
        // 转换员工总数
        list_resp->count = ntohs(list_resp->count);

        printf("--- Employee List (%hu records) ---\n", list_resp->count);
        if (list_resp->count == 0) {
            printf("No employees to list.\n");
        } else {
            // 循环接收每个员工的数据结构
            for (uint16_t i = 0; i < list_resp->count; ++i) {
                struct employee_t
                    employee_data;  // 声明一个临时 employee_t 结构体来接收
                if (read_full(fd, &employee_data, sizeof(struct employee_t)) ==
                    STATUS_ERROR) {
                    perror("read_full employee data");
                    return STATUS_ERROR;
                }
                // 转换员工数据中的字段（例如 hours）
                employee_data.hours = ntohl(employee_data.hours);

                printf("Employee #%d:\n", i + 1);
                printf("\tName: %s\n", employee_data.name);
                printf("\tAddress: %s\n", employee_data.address);
                printf("\tHours: %u\n", employee_data.hours);
            }
        }
        printf("----------------------------------\n\n");
        return STATUS_SUCCESS;

    } else {
        fprintf(stderr,
                "Unexpected message type for list employee response: %d\n",
                hdr->type);
        return STATUS_ERROR;
    }
}

/**
 * @brief 客户端发送删除最后一个员工的请求，并接收响应。
 * @param fd 服务器的套接字文件描述符。
 * @return 成功时返回 STATUS_SUCCESS，错误时返回 STATUS_ERROR。
 */
int send_remove_employee_req(int fd) {
    char buf[CLIENT_BUFFER_SIZE] = {0};
    dbproto_hdr_t *hdr = (dbproto_hdr_t *)buf;

    // 构造删除员工请求头部
    hdr->type = MSG_EMPLOYEE_DEL_REQ;
    hdr->len = 0;  // 删除请求（删除最后一个）没有消息体

    // 转换为网络字节序
    hdr->type = htonl(hdr->type);
    hdr->len = htons(hdr->len);

    // 发送完整的删除员工请求消息
    if (send_full(fd, buf, sizeof(dbproto_hdr_t)) == STATUS_ERROR) {
        perror("send_full remove employee request");
        return STATUS_ERROR;
    }

    // 接收服务器响应头部
    if (read_full(fd, buf, sizeof(dbproto_hdr_t)) == STATUS_ERROR) {
        perror("read_full remove employee response header");
        return STATUS_ERROR;
    }
    // 转换回主机字节序
    hdr->type = ntohl(hdr->type);
    hdr->len = ntohs(hdr->len);

    // 根据响应类型处理
    if (hdr->type == MSG_ERROR) {
        printf("Server returned an error for remove employee.\n");
        return STATUS_ERROR;
    } else if (hdr->type == MSG_EMPLOYEE_DEL_RESP) {
        // 验证响应体长度
        if (hdr->len != sizeof(dbproto_employee_del_resp_t)) {
            fprintf(stderr,
                    "Error: Remove employee response length mismatch.\n");
            return STATUS_ERROR;
        }
        // 接收响应体
        dbproto_employee_del_resp_t *del_resp =
            (dbproto_employee_del_resp_t *)(buf + sizeof(dbproto_hdr_t));
        if (read_full(fd, del_resp, sizeof(dbproto_employee_del_resp_t)) ==
            STATUS_ERROR) {
            perror("read_full remove employee response payload");
            return STATUS_ERROR;
        }
        // 转换状态码
        del_resp->status = ntohl(del_resp->status);
        if (del_resp->status == STATUS_SUCCESS) {
            printf("Employee removed successfully.\n");
            return STATUS_SUCCESS;
        } else {
            fprintf(stderr,
                    "Failed to remove employee on server (status: %d).\n",
                    del_resp->status);
            return STATUS_ERROR;
        }
    } else {
        fprintf(stderr,
                "Unexpected message type for remove employee response: %d\n",
                hdr->type);
        return STATUS_ERROR;
    }
}

/**
 * @brief 客户端程序主函数。
 *        解析命令行参数，连接服务器，并根据参数执行指定操作。
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * @return 成功时返回 STATUS_SUCCESS (0)，错误时返回 STATUS_ERROR (-1)。
 */
int main(int argc, char *argv[]) {
    // 使用 cleanup 宏，确保 fd 在函数结束时被关闭
    int fd __attribute__((cleanup(_cleanup_fd_))) = -1;
    char *addarg = NULL;
    char *portarg = NULL, *hostarg = NULL;
    unsigned short port = 0;
    bool list_flag = false;    // 标志：是否执行列出员工操作
    bool remove_flag = false;  // 标志：是否执行删除员工操作

    int c;
    // 解析命令行参数：支持 -p (端口), -h (主机), -a (添加), -l (列出), -r
    // (删除)
    while ((c = getopt(argc, argv, "p:h:a:lr")) != -1) {
        switch (c) {
            case 'a':  // 添加员工
                addarg = optarg;
                break;
            case 'p':  // 服务器端口
                portarg = optarg;
                if (portarg != NULL) { port = (unsigned short)atoi(portarg); }
                break;
            case 'h':  // 服务器主机地址
                hostarg = optarg;
                break;
            case 'l':  // 列出员工
                list_flag = true;
                break;
            case 'r':  // 删除员工
                remove_flag = true;
                break;
            case '?':  // 未知选项
                fprintf(stderr, "Error: Unknown option '-%c'\n", optopt);
                return STATUS_ERROR;
            default: return STATUS_ERROR;
        }
    }

    // 检查客户端操作的有效性：只能执行一个操作
    int action_count = (addarg != NULL) + list_flag + remove_flag;
    if (action_count > 1) {
        fprintf(stderr,
                "Error: Client can only perform one action at a time (-a, -l, "
                "or -r).\n");
        return STATUS_ERROR;
    }
    if (action_count == 0) {
        fprintf(stderr, "Error: No action specified (-a, -l, or -r).\n");
        return STATUS_ERROR;
    }

    // 检查端口参数
    if (port == 0) {
        fprintf(stderr, "Error: Invalid or missing port with -p option.\n");
        return STATUS_ERROR;
    }

    // 检查主机参数
    if (hostarg == NULL) {
        fprintf(stderr, "Error: Must specify host with -h option.\n");
        return STATUS_ERROR;
    }

    // 配置服务器地址结构体
    struct sockaddr_in serverInfo = {0};
    serverInfo.sin_family = AF_INET;

    // 将主机地址字符串转换为网络地址，使用 inet_pton 更安全
    if (inet_pton(AF_INET, hostarg, &(serverInfo.sin_addr)) != 1) {
        fprintf(stderr, "Error: Invalid host address '%s'.\n", hostarg);
        return STATUS_ERROR;
    }
    serverInfo.sin_port = htons(port);  // 端口转换为网络字节序

    // 创建套接字
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("socket");
        return STATUS_ERROR;
    }

    // 连接服务器
    if (connect(fd, (struct sockaddr *)&serverInfo, sizeof(serverInfo)) == -1) {
        perror("connect");
        return STATUS_ERROR;
    }
    printf("Successfully connected to %s:%u\n", hostarg, port);

    // 发送 Hello 请求进行协议协商
    if (send_hello(fd) != STATUS_SUCCESS) { return STATUS_ERROR; }

    // 根据命令行参数执行相应的操作
    if (addarg) {
        if (send_add_employee_req(fd, addarg) != STATUS_SUCCESS) {
            return STATUS_ERROR;
        }
    } else if (list_flag) {
        if (send_list_employee_req(fd) != STATUS_SUCCESS) {
            return STATUS_ERROR;
        }
    } else if (remove_flag) {
        if (send_remove_employee_req(fd) != STATUS_SUCCESS) {
            return STATUS_ERROR;
        }
    }

    printf("Client operations finished.\n");
    return STATUS_SUCCESS;  // fd 会被 cleanup 宏自动关闭
}