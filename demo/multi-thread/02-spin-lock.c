/*
demo for spin lock.
*/

#include <stdio.h>    // For printf, fprintf, perror
#include <pthread.h>  // For pthread_t, pthread_create, pthread_join, pthread_spin_*
#include <stdlib.h>   // For exit
#include <unistd.h>   // For sleep

#define THREAD_COUNT 10 // 线程数量
#define INCREMENT_COUNT 1000000 // 每个线程递增计数器的次数

int counter = 0; // 全局共享计数器
pthread_spinlock_t counter_lock; // 自旋锁变量

/**
 * @brief 线程执行的目标函数。
 *        每个线程都会尝试递增全局计数器 1,000,000 次，使用自旋锁保护临界区。
 */
void *thread_target() {
    // 尝试获取自旋锁
    // 如果锁已被持有，当前线程会忙等 (spin)，不断检查锁的状态
    if (pthread_spin_lock(&counter_lock) != 0) {
        perror("pthread_spin_lock");
        // 实际应用中可能需要更优雅的错误处理，比如退出线程
        pthread_exit(NULL);
    }

    // 临界区：安全地递增共享计数器
    for (int i = 0; i < INCREMENT_COUNT; ++i) {
        ++counter;
    }

    // 为了跟mutex做性能比较，两个工作时间都延长。
    sleep(1);

    // 释放自旋锁
    if (pthread_spin_unlock(&counter_lock) != 0) {
        perror("pthread_spin_unlock");
        pthread_exit(NULL);
    }
    
    // 打印当前计数器值。注意：此打印操作不在锁的保护范围内。
    // 因此，不同线程打印出的值可能不同，但最终值将是准确的。
    printf("Thread finished. Current counter value (might be updated by others): %d\n", counter);

    pthread_exit(NULL); // 线程正常退出
}

/**
 * @brief 主函数，创建线程，初始化和销毁自旋锁，并等待线程完成。
 */
int main() {
    pthread_t threads[THREAD_COUNT]; // 线程 ID 数组
    int ret; // 用于存储 pthread 函数的返回值

    // 初始化自旋锁
    // pshared 参数为 0 表示锁在当前进程内的线程间共享
    if (pthread_spin_init(&counter_lock, 0) != 0) {
        perror("pthread_spin_init");
        exit(EXIT_FAILURE); // 初始化失败是致命错误
    }

    // 创建线程
    for (int i = 0; i < THREAD_COUNT; ++i) {
        ret = pthread_create(&threads[i], NULL, thread_target, NULL);
        if (ret != 0) {
            fprintf(stderr, "Error creating thread %d: %d\n", i, ret);
            // 实际应用中需要清理已创建的线程和锁
            pthread_spin_destroy(&counter_lock);
            exit(EXIT_FAILURE);
        }
    }

    // 等待所有线程完成
    for (int i = 0; i < THREAD_COUNT; ++i) {
        ret = pthread_join(threads[i], NULL);
        if (ret != 0) {
            fprintf(stderr, "Error joining thread %d: %d\n", i, ret);
            // 实际应用中需要记录错误，并可能采取恢复措施
        }
    }

    // 销毁自旋锁，释放其占用的资源
    if (pthread_spin_destroy(&counter_lock) != 0) {
        perror("pthread_spin_destroy");
        exit(EXIT_FAILURE);
    }
    
    // 打印最终的计数器值，验证其正确性
    printf("All threads finished. Final counter value: %d\n", counter);

    return 0; // 主线程正常退出
}