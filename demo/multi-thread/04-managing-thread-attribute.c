/*
demo for managing thread attribute.
*/

// 必须在所有 #include 之前定义 _GNU_SOURCE 宏
// 否则，GNU 扩展的函数声明 (如 CPU 亲和性相关的函数) 不会被编译器看到。
#define _GNU_SOURCE 

#include <stdio.h>    // For printf, fprintf, perror
#include <stdlib.h>   // For exit, EXIT_FAILURE
#include <string.h>   // For strerror
#include <errno.h>    // For errno

#include <pthread.h>  // For pthread_*, pthread_attr_t, etc.
#include <unistd.h>   // For sleep (optional), used in thread_function
#include <sched.h>    // For cpu_set_t, CPU_ZERO, CPU_SET, CPU_ISSET, sched_param, sched_get_priority_max (核心修正: 确保包含此头文件)

// 辅助函数：打印 pthread 函数的错误信息并退出
static void handle_pthread_error(const char *func_name, int err_code) {
    fprintf(stderr, "Error in %s: %d (%s)\n", func_name, err_code, strerror(err_code)); // 修正 strerror 的使用
    exit(EXIT_FAILURE);
}

/**
 * @brief 线程执行的目标函数。
 *        获取并打印当前线程的 ID 及其 CPU 亲和性设置。
 */
void *thread_function() {
    pthread_t tid = pthread_self(); // 获取当前线程的 ID
    cpu_set_t cpuset;               // CPU 集合变量
    int ret;

    CPU_ZERO(&cpuset); // CPU_ZERO, CPU_SET 等宏需要 <sched.h> 且 _GNU_SOURCE

    // 获取当前线程的 CPU 亲和性
    // tid: 目标线程ID (pthread_self() 获取自身)
    // sizeof(cpu_set_t): cpuset 的大小 (正确用法)
    // &cpuset: 存储亲和性信息的结构体地址
    ret = pthread_getaffinity_np(tid, sizeof(cpu_set_t), &cpuset); // _GNU_SOURCE 和 <sched.h>
    if (ret != 0) {
        // pthread_getaffinity_np 失败时，会返回错误码，而不是设置 errno
        handle_pthread_error("pthread_getaffinity_np", ret); 
        pthread_exit(NULL);
    }

    printf("Thread %lu running on CPUs: ", (unsigned long)tid);
    // 遍历所有可能的 CPU，检查线程被设置到哪些 CPU 上
    for (int i = 0; i < CPU_SETSIZE; ++i) { 
        if (CPU_ISSET(i, &cpuset)) {
            printf("%d ", i);
        }
    }
    printf("\n");

    pthread_exit(NULL); // 线程正常退出
}

/**
 * @brief 主函数，演示如何使用 pthread_attr_t 来管理线程属性。
 *        设置栈大小、调度策略、优先级和 CPU 亲和性，然后创建线程。
 */
int main() {
    pthread_t thread;          // 线程 ID 变量
    pthread_attr_t attr;       // 线程属性对象
    cpu_set_t cpuset;          // CPU 集合，用于设置亲和性
    struct sched_param param;  // 调度参数，用于设置优先级
    int ret;                   // 用于存储 pthread 函数的返回值

    // 1. 初始化线程属性对象
    ret = pthread_attr_init(&attr);
    if (ret != 0) {
        handle_pthread_error("pthread_attr_init", ret);
    }

    // 2. 设置栈大小 (Stack Size)
    size_t stacksize = 1024 * 1024; // 1 MB 栈
    ret = pthread_attr_setstacksize(&attr, stacksize);
    if (ret != 0) {
        handle_pthread_error("pthread_attr_setstacksize", ret);
    } else {
        printf("Set stack size to %zu bytes.\n", stacksize);
    }

    // 3. 设置调度策略 (Scheduling Policy) 和优先级 (Priority)
    // SCHED_RR (Round Robin) 和 SCHED_FIFO (First In, First Out) 是实时调度策略
    // 这些通常需要 root 权限或 CAP_SYS_NICE 功能才能成功设置和生效
    // pthread_attr_setschedpolicy 的第一个参数需要 &attr (地址)
    ret = pthread_attr_setschedpolicy(&attr, SCHED_RR);
    if (ret != 0) {
        handle_pthread_error("pthread_attr_setschedpolicy", ret);
    } else {
        printf("Set scheduling policy to SCHED_RR.\n");
    }

    // 获取 SCHED_RR 的最大优先级
    param.sched_priority = sched_get_priority_max(SCHED_RR);
    ret = pthread_attr_setschedparam(&attr, &param);
    if (ret != 0) {
        handle_pthread_error("pthread_attr_setschedparam", ret);
    } else {
        printf("Set scheduling priority to max (%d).\n", param.sched_priority);
    }
    
    // 4. 设置调度继承属性 (Inherit Scheduler)
    // PTHREAD_EXPLICIT_SCHED 表示线程的调度策略和优先级由 attr 指定，而不是继承父线程的
    ret = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    if (ret != 0) {
        handle_pthread_error("pthread_attr_setinheritsched", ret);
    } else {
        printf("Set inherit scheduler to PTHREAD_EXPLICIT_SCHED.\n");
    }

    // 5. 设置 CPU 亲和性 (CPU Affinity)
    // 将线程绑定到特定的 CPU 核。CPU_SET(0, &cpuset) 绑定到 CPU 0。
    // 这也是 GNU 扩展，通常需要特殊权限。
    CPU_ZERO(&cpuset); // 清空 CPU 集合
    CPU_SET(0, &cpuset); // 将 CPU 0 加入集合 (核心修正: CPU_SET 需要 <sched.h> 且 _GNU_SOURCE)
    // pthread_attr_setaffinity_np 的第二个参数应为 sizeof(cpu_set_t) (size_t)
    ret = pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpuset); 
    if (ret != 0) {
        // EINVAL 错误可能表示 CPU_SETSIZE 参数不正确，或系统不支持，或权限不足
        // 建议使用 sizeof(cpu_set_t) 作为 size 参数
        handle_pthread_error("pthread_attr_setaffinity_np", ret);
    } else {
        printf("Set CPU affinity to CPU 0.\n");
    }

    // 6. 使用这些属性创建线程
    ret = pthread_create(&thread, &attr, thread_function, NULL);
    if (ret != 0) {
        handle_pthread_error("pthread_create", ret);
        pthread_attr_destroy(&attr); // 创建失败也要销毁属性对象
        exit(EXIT_FAILURE);
    }

    // 7. 等待子线程完成
    ret = pthread_join(thread, NULL);
    if (ret != 0) {
        handle_pthread_error("pthread_join", ret);
    }

    // 8. 销毁线程属性对象，释放其占用的资源
    ret = pthread_attr_destroy(&attr);
    if (ret != 0) {
        handle_pthread_error("pthread_attr_destroy", ret);
    }

    printf("Main thread: Thread attributes demo completed.\n");

    return 0; // 主线程正常退出
}