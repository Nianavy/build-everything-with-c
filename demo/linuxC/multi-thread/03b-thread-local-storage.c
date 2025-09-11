/*
demo for thread local storage.
*/

#include <pthread.h>  // For pthread_t, pthread_create, pthread_join, pthread_self
#include <stdio.h>    // For printf, fprintf, perror
#include <stdlib.h>  // For exit
#include <unistd.h>  // For sleep (optional, for demo clarity)

#define THREAD_COUNT 10  // 线程数量

// 使用 __thread 关键字声明线程局部存储变量
// 每个线程都将拥有它自己独立的 x_thd 副本，并自动初始化为 0
// (对于全局/静态存储期变量)
__thread int x_thd = 0;  // 显式初始化为 0，清晰起见

/**
 * @brief 线程执行的目标函数。
 *        每个线程递增自己的 x_thd 副本，并打印线程ID和 x_thd 的值。
 */
void *wait_for_event() {
    pthread_t current_tid = pthread_self();  // 获取当前线程的ID

    printf("Thread %lu: Initial x_thd = %d\n", (unsigned long)current_tid,
           x_thd);

    // 递增当前线程的 x_thd 副本
    ++x_thd;

    // 再次打印，显示递增后的值
    printf("Thread %lu: Incremented x_thd = %d\n", (unsigned long)current_tid,
           x_thd);

    // 稍微延迟一下，让输出更容易观察（可选）
    // usleep(100);

    pthread_exit(NULL);  // 线程正常退出
}

/**
 * @brief 主函数，创建线程并等待它们完成。
 */
int main() {
    pthread_t threads[THREAD_COUNT];  // 线程 ID 数组
    int ret;                          // 用于存储 pthread 函数的返回值

    // 主线程的 x_thd 副本 (与子线程独立)
    printf("Main thread: Initial x_thd = %d\n", x_thd);
    x_thd = 100;  // 主线程可以修改自己的 x_thd 副本，不会影响子线程
    printf("Main thread: Modified x_thd = %d\n", x_thd);

    // 创建线程
    for (int i = 0; i < THREAD_COUNT; ++i) {
        ret = pthread_create(&threads[i], NULL, wait_for_event, NULL);
        if (ret != 0) {
            fprintf(stderr, "Error creating thread %d: %d\n", i, ret);
            exit(EXIT_FAILURE);
        }
    }

    // 等待所有线程完成
    for (int i = 0; i < THREAD_COUNT; ++i) {
        ret = pthread_join(threads[i], NULL);
        if (ret != 0) {
            fprintf(stderr, "Error joining thread %d: %d\n", i, ret);
        }
    }

    // 再次打印主线程的 x_thd 副本，证明它未被子线程修改
    printf("Main thread: After all threads, x_thd = %d\n", x_thd);

    return 0;  // 主线程正常退出
}