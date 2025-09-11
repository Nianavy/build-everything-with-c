/*
 * deadlock_unified_demo.c
 * 演示经典的死锁场景和通过资源有序分配避免死锁的场景。
 * 通过在编译时定义 DEMO_DEADLOCK 宏来切换演示模式：
 *   不定义 DEMO_DEADLOCK 或定义为 0 -> 演示死锁避免 (默认行为)
 *   定义 DEMO_DEADLOCK 为 1         -> 演示死锁
 *
 * 编译死锁演示: gcc -Wall -Wextra -DDEMO_DEADLOCK=1 deadlock_unified_demo.c -o
 * deadlock_unified_demo -pthread 运行: ./deadlock_unified_demo
 * (程序会死锁，需要 Ctrl+C 终止)
 *
 * 编译死锁避免演示: gcc -Wall -Wextra deadlock_unified_demo.c -o
 * deadlock_unified_demo -pthread 运行: ./deadlock_unified_demo (程序将正常完成)
 */

#include <errno.h>  // **核心修正: 包含 errno.h 以声明 errno 宏**
#include <pthread.h>
#include <stdarg.h>  // For va_list, va_start, va_end (用于可变参数辅助函数)
#include <stdio.h>
#include <stdlib.h>  // For exit
#include <string.h>  // For strerror
#include <unistd.h>  // For sleep

// --- 默认行为：死锁避免 ---
// 如果没有在命令行定义 DEMO_DEADLOCK，或者定义为 0，则执行死锁避免逻辑
#ifndef DEMO_DEADLOCK
#define DEMO_DEADLOCK 0
#endif
// ----------------------------

// 定义两个互斥量，它们将是我们的共享资源
pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief 辅助函数：处理 pthread 函数的错误，接受可变参数以打印更丰富的上下文。
 *        pthread 函数失败时返回错误码 (非零值)，strerror 可以解析这些错误码。
 * @param err_code pthread 函数返回的错误码。
 * @param fmt 格式化字符串，类似于 printf。
 * @param ... 可变参数，用于填充格式化字符串。
 */
static void handle_pthread_error(int err_code, const char *fmt, ...) {
    va_list args;
    fprintf(stderr, "Error: ");
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);  // 使用 vfprintf 打印可变参数
    va_end(args);
    // 修正：strerror 应该解析 pthread 函数返回的 err_code，而不是全局的 errno
    fprintf(stderr, " (pthread error code %d: %s)\n", err_code,
            strerror(err_code));
    exit(EXIT_FAILURE);
}

/**
 * @brief 线程任务函数。
 *        根据 DEMO_DEADLOCK 宏的不同，它会以不同的顺序获取锁。
 * @param arg 线程 ID (用于区分打印信息)。
 */
void *thread_func(void *arg) {
    int thread_id = *(int *)arg;
    int ret;

    printf("Thread %d: Starting...\n", thread_id);

#if DEMO_DEADLOCK == 1  // 死锁演示模式：线程以相反的顺序获取锁
    if (thread_id == 1) {  // 线程 A 模仿：mutex1 -> mutex2
        printf("Thread %d: Attempting to lock mutex1...\n", thread_id);
        ret = pthread_mutex_lock(&mutex1);
        if (ret != 0)
            handle_pthread_error(
                ret, "Thread %d: pthread_mutex_lock (mutex1) failed.",
                thread_id);
        printf(
            "Thread %d: Locked mutex1. Doing some work, then attempting to "
            "lock mutex2...\n",
            thread_id);
        sleep(1);

        printf("Thread %d: Attempting to lock mutex2...\n", thread_id);
        ret = pthread_mutex_lock(&mutex2);
        if (ret != 0)
            handle_pthread_error(
                ret, "Thread %d: pthread_mutex_lock (mutex2) failed.",
                thread_id);
        printf("Thread %d: Locked mutex2. Critical section for Thread %d.\n",
               thread_id, thread_id);

        printf("Thread %d: Releasing mutex2...\n", thread_id);
        ret = pthread_mutex_unlock(&mutex2);
        if (ret != 0)
            handle_pthread_error(
                ret, "Thread %d: pthread_mutex_unlock (mutex2) failed.",
                thread_id);
        printf("Thread %d: Releasing mutex1...\n", thread_id);
        ret = pthread_mutex_unlock(&mutex1);
        if (ret != 0)
            handle_pthread_error(
                ret, "Thread %d: pthread_mutex_unlock (mutex1) failed.",
                thread_id);
    } else {  // 线程 B 模仿：mutex2 -> mutex1
        printf("Thread %d: Attempting to lock mutex2...\n", thread_id);
        ret = pthread_mutex_lock(&mutex2);
        if (ret != 0)
            handle_pthread_error(
                ret, "Thread %d: pthread_mutex_lock (mutex2) failed.",
                thread_id);
        printf(
            "Thread %d: Locked mutex2. Doing some work, then attempting to "
            "lock mutex1...\n",
            thread_id);
        sleep(1);

        printf("Thread %d: Attempting to lock mutex1...\n", thread_id);
        ret = pthread_mutex_lock(&mutex1);
        if (ret != 0)
            handle_pthread_error(
                ret, "Thread %d: pthread_mutex_lock (mutex1) failed.",
                thread_id);
        printf("Thread %d: Locked mutex1. Critical section for Thread %d.\n",
               thread_id, thread_id);

        printf("Thread %d: Releasing mutex1...\n", thread_id);
        ret = pthread_mutex_unlock(&mutex1);
        if (ret != 0)
            handle_pthread_error(
                ret, "Thread %d: pthread_mutex_unlock (mutex1) failed.",
                thread_id);
        printf("Thread %d: Releasing mutex2...\n", thread_id);
        ret = pthread_mutex_unlock(&mutex2);
        if (ret != 0)
            handle_pthread_error(
                ret, "Thread %d: pthread_mutex_unlock (mutex2) failed.",
                thread_id);
    }

#else  // 死锁避免模式 (DEMO_DEADLOCK == 0)：所有线程都按照约定顺序 (mutex1 ->
       // mutex2) 获取锁
    printf("Thread %d: Attempting to lock mutex1...\n", thread_id);
    ret = pthread_mutex_lock(&mutex1);  // 始终先获取 mutex1
    if (ret != 0)
        handle_pthread_error(
            ret, "Thread %d: pthread_mutex_lock (mutex1) failed.", thread_id);
    printf(
        "Thread %d: Locked mutex1. Doing some work, then attempting to lock "
        "mutex2...\n",
        thread_id);
    sleep(1);  // 模拟一些工作

    printf("Thread %d: Attempting to lock mutex2...\n", thread_id);
    ret = pthread_mutex_lock(&mutex2);  // 再获取 mutex2
    if (ret != 0)
        handle_pthread_error(
            ret, "Thread %d: pthread_mutex_lock (mutex2) failed.", thread_id);
    printf("Thread %d: Locked mutex2. Critical section for Thread %d.\n",
           thread_id, thread_id);

    printf("Thread %d: Releasing mutex2...\n", thread_id);
    ret = pthread_mutex_unlock(&mutex2);
    if (ret != 0)
        handle_pthread_error(
            ret, "Thread %d: pthread_mutex_unlock (mutex2) failed.", thread_id);
    printf("Thread %d: Releasing mutex1...\n", thread_id);
    ret = pthread_mutex_unlock(&mutex1);
    if (ret != 0)
        handle_pthread_error(
            ret, "Thread %d: pthread_mutex_unlock (mutex1) failed.", thread_id);
#endif

    printf("Thread %d: Finished.\n", thread_id);
    return NULL;
}

int main() {
    pthread_t tid_A, tid_B;
    int id_A = 1, id_B = 2;  // 用于传递给线程函数的 ID
    int ret_A, ret_B;

#if DEMO_DEADLOCK == 1
    printf("--- DEADLOCK DEMONSTRATION ---\n");
    printf(
        "This program is expected to deadlock. Press Ctrl+C to terminate.\n");
#else
    printf(
        "--- DEADLOCK AVOIDANCE DEMONSTRATION (Ordered Resource Allocation) "
        "---\n");
    printf("This program is expected to complete normally.\n");
#endif

    // 创建两个线程
    ret_A = pthread_create(&tid_A, NULL, thread_func, &id_A);
    if (ret_A != 0)
        handle_pthread_error(ret_A, "pthread_create (Thread 1) failed.");

    ret_B = pthread_create(&tid_B, NULL, thread_func, &id_B);
    if (ret_B != 0)
        handle_pthread_error(ret_B, "pthread_create (Thread 2) failed.");

    // 等待线程完成
    // 在死锁模式下，pthread_join 会一直等待，直到被外部终止
    ret_A = pthread_join(tid_A, NULL);
    if (ret_A != 0)
        handle_pthread_error(ret_A, "pthread_join (Thread 1) failed.");

    ret_B = pthread_join(tid_B, NULL);
    if (ret_B != 0)
        handle_pthread_error(ret_B, "pthread_join (Thread 2) failed.");

#if DEMO_DEADLOCK == 1
    printf(
        "--- PROGRAM UNEXPECTEDLY FINISHED (NO DEADLOCK), SOMETHING IS WRONG "
        "---\n");
#else
    printf("--- PROGRAM COMPLETED NORMALLY (NO DEADLOCK) ---\n");
#endif

    // 销毁互斥量 (在死锁演示中可能无法到达这里)
    ret_A = pthread_mutex_destroy(&mutex1);
    if (ret_A != 0)
        handle_pthread_error(ret_A, "pthread_mutex_destroy (mutex1) failed.");
    ret_B = pthread_mutex_destroy(&mutex2);
    if (ret_B != 0)
        handle_pthread_error(ret_B, "pthread_mutex_destroy (mutex2) failed.");

    return 0;
}