#include <stdio.h>
#include <stdbool.h>  // For bool, true, false
#include <getopt.h>   // For getopt 命令行参数解析
#include <stdlib.h>   // For exit, calloc, free
#include <unistd.h>   // For close, write, read
#include <string.h>   // For memset
#include <signal.h>   // For sigaction, SIGINT, sig_atomic_t
#include <errno.h>    // For errno, EINTR

#include "../../include/common.h" // 包含通用宏、协议结构和网络读写函数
#include "../../include/file.h"   // 包含文件操作函数
#include "../../include/parse.h"  // 包含数据库解析和员工结构
#include "../../include/srvpoll.h"// 包含服务器轮询和客户端状态管理

// 全局客户端状态数组，存储所有连接客户端的信息
clientstate_t clientStates[MAX_CLIENTS];

// 全局退出标志，volatile sig_atomic_t 确保在信号处理函数中安全修改
static volatile sig_atomic_t server_should_exit = 0;

/**
 * @brief SIGINT 信号处理函数。
 *        当收到 SIGINT (Ctrl+C) 信号时，设置退出标志，以便服务器优雅关闭。
 * @param sig 接收到的信号编号
 */
static void handle_sigint(int sig) {
    if (sig == SIGINT) {
        fprintf(stderr, "\nSIGINT received. Initiating graceful shutdown...\n");
        server_should_exit = 1; // 设置退出标志
    }
}

/**
 * @brief 打印服务器程序的命令行用法。
 * @param argv 程序参数数组
 */
void print_usage(char *argv[]) {
    fprintf(stderr, "Usage: %s -f <database file> -p <port> [options]\n", argv[0]);
    fprintf(stderr, "\t -n - create new database file (if not exists)\n");
    fprintf(stderr, "\t -f - (required) path to database file\n");
    fprintf(stderr, "\t -a <\"Name-Address-Hours\"> - add a new employee (only for non-server mode)\n");
    fprintf(stderr, "\t -l - list all employees (only for non-server mode)\n");
    fprintf(stderr, "\t -r - remove the last employee (only for non-server mode)\n");
    fprintf(stderr, "\t -p - (required) port for the server to listen on\n");
    return;
}

/**
 * @brief 填充 pollfd 结构体数组，准备调用 poll()。
 *        监听套接字放在 fds[0]，活跃客户端套接字按顺序添加。
 * @param fds pollfd 数组
 * @param listen_fd 监听套接字文件描述符
 * @param clientStates 客户端状态数组
 * @param max_clients 客户端状态数组的最大大小
 * @param current_nfds 指向当前活跃文件描述符数量的指针
 */
static void fill_pollfds(struct pollfd *fds, int listen_fd, const clientstate_t *clientStates, int max_clients, int *current_nfds) {
    memset(fds, 0, (max_clients + 1) * sizeof(struct pollfd)); // 每次重新填充
    int nfds_count = 0;

    // 添加监听套接字
    fds[nfds_count].fd = listen_fd;
    fds[nfds_count].events = POLLIN; // 关注可读事件
    nfds_count++;

    // 添加所有活跃的客户端套接字
    for (int i = 0; i < max_clients; ++i) {
        if (clientStates[i].fd != -1) {
            fds[nfds_count].fd = clientStates[i].fd;
            fds[nfds_count].events = POLLIN; // 关注可读事件
            nfds_count++;
        }
    }
    *current_nfds = nfds_count;
}

/**
 * @brief 服务器主循环，使用 poll() 进行 I/O 多路复用。
 *        处理新连接、接收客户端消息，并根据 FSM 转发处理。
 * @param port 服务器监听端口
 * @param dbhdr 指向数据库头部
 * @param employees_ptr 指向员工数组的指针（FSM 可能修改它）
 */
void poll_loop(unsigned short port, struct dbheader_t *dbhdr, struct employee_t **employees_ptr) {
    // 监听套接字文件描述符，使用 cleanup 宏确保自动关闭
    int listen_fd __attribute__((cleanup(_cleanup_fd_))) = -1;
    int conn_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    // pollfd 数组，用于传递给 poll()，大小为 MAX_CLIENTS + 1 (监听套接字)
    struct pollfd *fds __attribute__((cleanup(_cleanup_ptr_))) = calloc(MAX_CLIENTS + 1, sizeof(struct pollfd));
    if (fds == NULL) {
        perror("calloc for pollfds");
        exit(EXIT_FAILURE);
    }

    int nfds = 0; // 活跃文件描述符的数量
    int opt = 1;  // 用于 setsockopt

    // 初始化所有客户端状态槽位
    init_clients(clientStates, MAX_CLIENTS);

    // 创建监听套接字
    if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 设置套接字选项：允许地址重用，防止 TIME_WAIT 状态导致重启失败
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // 配置服务器地址结构体
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // 监听所有可用网络接口
    server_addr.sin_port = htons(port);       // 端口转换为网络字节序

    // 绑定套接字到指定地址和端口
    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // 启动监听，将套接字设为被动连接模式
    if (listen(listen_fd, 10) == -1) { // 监听队列长度为 10
        perror("listen");
        exit(EXIT_FAILURE);
    }
    printf("Server listening on port %d\n", port);

    // 服务器主循环：持续监听客户端连接和消息，直到收到退出信号
    while (!server_should_exit) {
        // 每次 poll 前都重新填充 fds 数组并计算 nfds
        fill_pollfds(fds, listen_fd, clientStates, MAX_CLIENTS, &nfds);

        // 调用 poll() 等待 I/O 事件，设置 100ms 超时，以便定期检查退出标志
        int n_events = poll(fds, nfds, 100); 
        if (n_events == -1) {
            // 如果 poll 被 SIGINT (中断信号) 中断，errno 会是 EINTR，此时不应退出，而是继续循环
            if (errno == EINTR) {
                continue; 
            }
            perror("poll"); // 其他错误是严重错误，退出
            exit(EXIT_FAILURE);
        }
        
        // 如果 poll 超时（n_events == 0），且收到退出信号，则退出循环
        if (n_events == 0 && server_should_exit) {
            break; 
        }

        // 处理监听套接字上的新连接
        // fds[0] 总是监听套接字
        if (fds[0].revents & POLLIN) {
            conn_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
            if (conn_fd == -1) {
                perror("accept"); // accept 错误，但不是致命错误，继续循环
            } else {
                printf("New connection from %s:%d\n",
                inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

                // 查找一个空闲的客户端状态槽位
                int freeSlot = find_free_slot(clientStates, MAX_CLIENTS);
                if (freeSlot == STATUS_ERROR) { // STATUS_ERROR == -1
                    printf("Server full: closing new connection (fd %d)\n", conn_fd);
                    close(conn_fd); // 服务器满，关闭新连接
                } else {
                    // 初始化新客户端的状态
                    clientStates[freeSlot].fd = conn_fd;
                    clientStates[freeSlot].state = STATE_CONNECTED; // 初始状态为 CONNECTED，等待 Hello
                    clientStates[freeSlot].buffer_pos = 0;
                    clientStates[freeSlot].msg_expected_len = 0;
                    printf("Client fd %d assigned to slot %d. State: CONNECTED\n", conn_fd, freeSlot);
                }
            }
        }

        // 遍历所有客户端状态，处理它们的事件
        // 注意：这里的循环顺序遍历 clientStates 数组，然后查找对应的 fds 条目。
        // 对于大量客户端，更高效的方式是在 fill_pollfds 时，将 fds 索引映射回 clientStates 索引。
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (clientStates[i].fd != -1) { // 如果客户端是活跃的
                for (int j = 1; j < nfds; ++j) { // 从 fds[1] 开始遍历客户端的 fd (fds[0] 是监听套接字)
                    if (fds[j].fd == clientStates[i].fd) { // 找到对应的 pollfd 条目
                        if (fds[j].revents & POLLIN) { // 如果有可读事件
                            // 调用客户端 FSM 处理接收到的数据
                            handle_client_fsm(dbhdr, employees_ptr, &clientStates[i]);
                        }
                        // 如果有其他事件，例如 POLLOUT，也可以在这里处理
                        break; // 找到并处理了该客户端的事件，跳出内层循环
                    }
                }
            }
        }
    }
    printf("Poll loop exited gracefully.\n");
    // fds 内存和 listen_fd 文件描述符会在这里被 cleanup 宏自动处理
}

/**
 * @brief 服务器程序主函数。
 *        解析命令行参数，初始化数据库，启动服务器循环或执行单次文件操作。
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * @return 成功时返回 STATUS_SUCCESS (0)，错误时返回 STATUS_ERROR (-1)。
 */
int main(int argc, char *argv[]) {
    // 自动资源清理：文件描述符、数据库头部内存、员工数组内存
    int dbfd __attribute__((cleanup(_cleanup_fd_))) = -1;
    struct dbheader_t *dbhdr __attribute__((cleanup(_cleanup_ptr_))) = NULL;
    struct employee_t *employees __attribute__((cleanup(_cleanup_ptr_))) = NULL;

    char *filepath = NULL;
    char *portarg = NULL;
    unsigned short server_port = 0; // 服务器监听端口，避免与 PROTO_VER 的 PORT 宏混淆
    char *addstring = NULL;
    bool newfile = false;
    int c;
    bool list_employees_flag = false;
    bool remove_employee_flag = false;
    bool run_server_mode = false; // 标志：区分是执行单次命令行操作还是启动服务器

    // 解析命令行参数
    while ((c = getopt(argc, argv, "nf:p:a:lr")) != -1) {
        switch (c) {
            case 'n': // 创建新数据库文件
                newfile = true;
                break;
            case 'f': // 数据库文件路径
                filepath = optarg;
                break;
            case 'a': // 添加员工（仅用于非服务器模式）
                addstring = optarg;
                break;
            case 'l': // 列出员工（仅用于非服务器模式）
                list_employees_flag = true;
                break;
            case 'r': // 删除员工（仅用于非服务器模式）
                remove_employee_flag = true;
                break;
            case 'p': // 服务器监听端口
                portarg = optarg;
                if (portarg != NULL) {
                    server_port = (unsigned short)atoi(portarg);
                    run_server_mode = true; // 如果指定了端口，默认进入服务器模式
                }
                break;
            case '?': // 未知选项
                fprintf(stderr, "Error: Unknown option '-%c'\n", optopt);
                print_usage(argv);
                return STATUS_ERROR;
            default:
                return STATUS_ERROR;
        }
    }

    // 检查数据库文件路径是否提供
    if (filepath == NULL) {
        fprintf(stderr, "Error: Filepath is a required argument\n");
        print_usage(argv);
        return STATUS_ERROR;
    }

    // 根据 newfile 标志创建或打开数据库文件
    if (newfile) {
        dbfd = create_db_file(filepath);
        if (dbfd == STATUS_ERROR) {
            fprintf(stderr, "Error: Unable to create database file '%s'\n", filepath);
            return STATUS_ERROR;
        }
        if (create_db_header(dbfd, &dbhdr) == STATUS_ERROR) {
            fprintf(stderr, "Error: Failed to create database header for '%s'\n", filepath);
            return STATUS_ERROR; // dbfd 会被 cleanup 关闭
        }
    } else {
        dbfd = open_db_file(filepath);
        if (dbfd == STATUS_ERROR) {
            fprintf(stderr, "Error: Unable to open database file '%s'\n", filepath);
            return STATUS_ERROR;
        }
        if (validate_db_header(dbfd, &dbhdr) == STATUS_ERROR) {
            fprintf(stderr, "Error: Failed to validate database header for '%s'\n", filepath);
            return STATUS_ERROR; // dbfd 会被 cleanup 关闭
        }
    }

    // 读取所有员工数据到内存
    if (read_employees(dbfd, dbhdr, &employees) != STATUS_SUCCESS) {
        fprintf(stderr, "Error: Failed to read employees from '%s'\n", filepath);
        return STATUS_ERROR; // dbhdr, employees, dbfd 会被 cleanup 关闭/free
    }

    // 根据是否为服务器模式，执行不同的逻辑
    if (!run_server_mode) {
        // 非服务器模式：执行单次命令行数据库操作
        if (addstring) {
            if (add_employee(dbhdr, &employees, addstring) != STATUS_SUCCESS) {
                fprintf(stderr, "Error: Failed to add employee.\n");
                return STATUS_ERROR;
            }
        }
        if (remove_employee_flag) {
            if (remove_employee(dbhdr, &employees) != STATUS_SUCCESS) {
                fprintf(stderr, "Error: Failed to remove employee.\n");
                return STATUS_ERROR;
            }
        }
        if (list_employees_flag) {
            if (list_employees(dbhdr, employees) != STATUS_SUCCESS) {
                fprintf(stderr, "Error: Failed to list employees.\n");
                return STATUS_ERROR;
            }
        }
        // 非服务器模式下，操作完成后立即写入文件并退出
        if (output_file(dbfd, dbhdr, employees) != STATUS_SUCCESS) {
            fprintf(stderr, "Error: Failed to output file '%s' after operations.\n", filepath);
            return STATUS_ERROR;
        }
        printf("Non-server operations finished. Database updated.\n");
    } else {
        // 服务器模式：启动网络监听循环
        if (server_port == 0) { // 再次检查端口是否有效
            fprintf(stderr, "Error: Server mode requires a valid port with -p.\n");
            print_usage(argv);
            return STATUS_ERROR;
        }
        
        // 注册 SIGINT (Ctrl+C) 信号处理函数，以便优雅关闭服务器
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = handle_sigint; // 指定信号处理函数
        sigaction(SIGINT, &sa, NULL); // 注册 SIGINT 处理器

        printf("Starting server on port %u...\n", server_port);
        // 进入服务器主循环
        poll_loop(server_port, dbhdr, &employees); // 传递 employees 指针的指针以便 FSM 可以修改它

        // 服务器退出后，保存内存中的数据到文件
        if (output_file(dbfd, dbhdr, employees) != STATUS_SUCCESS) {
            fprintf(stderr, "Error: Failed to output file '%s' after server shutdown.\n", filepath);
            return STATUS_ERROR;
        }
        printf("Server shutdown. Database updated.\n");
    }

    return STATUS_SUCCESS; // 所有资源 (dbfd, dbhdr, employees) 会在这里被 cleanup 宏自动处理
}