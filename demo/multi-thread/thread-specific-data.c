/*
demo for thread specific data.
*/

#include <stdio.h>    // For printf, fprintf, perror
#include <stdlib.h>   // For malloc, free, exit
#include <pthread.h>  // For pthread_key_t, pthread_key_create, pthread_setspecific, etc.

pthread_key_t key; // 全局键，用于标识线程特定数据

/**
 * @brief 线程特定数据的析构函数。
 *        当线程退出时，如果给定的键关联了数据，系统会自动调用此函数来清理数据。
 * @param arr 指向要释放的线程特定数据（这里是数组）的指针。
 */
void array_destructor(void *arr) {
    if (arr != NULL) { // 确保指针不为 NULL 才 free
        free(arr);
        printf("Array freed for a thread\n");
    }
    // 返回类型应为 void
} 

/**
 * @brief 线程执行的目标函数。
 *        每个线程都会分配一个自己的数组，将其与全局键关联，并初始化打印。
 */
void *thread_function() {
    // 线程私有数据：分配一个整数数组
    int *my_array = malloc(sizeof(int) * 10);
    if (my_array == NULL) {
        perror("malloc for thread-specific array");
        pthread_exit(NULL); // 内存分配失败，线程退出
    }

    // 将线程私有数据与全局键关联
    // 这样，当线程退出时，array_destructor 会被调用来 free my_array
    if (pthread_setspecific(key, my_array) != 0) {
        perror("pthread_setspecific");
        free(my_array); // 设置失败，手动 free 已分配的内存
        pthread_exit(NULL);
    }
    
    // 初始化线程私有数组
    for (int i = 0; i < 10; ++i) {
        my_array[i] = i;
    }

    // 打印线程私有数组的内容
    printf("Thread %lu: My array contents: ", (unsigned long)pthread_self()); // 打印线程ID以区分
    for (int i = 0; i < 10; ++i) {
        printf("%d ", my_array[i]);
    }
    printf("\n");

    pthread_exit(NULL); // 线程正常退出
}

/**
 * @brief 主函数，创建线程特定数据键，创建线程，等待线程完成，并销毁键。
 */
int main() {
    pthread_t thread1, thread2; // 线程 ID 变量
    int ret; // 用于存储 pthread 函数的返回值

    // 创建线程特定数据键，并指定析构函数
    if (pthread_key_create(&key, array_destructor) != 0) {
        perror("pthread_key_create");
        exit(EXIT_FAILURE); // 键创建失败是致命错误
    }

    // 创建第一个线程
    ret = pthread_create(&thread1, NULL, thread_function, NULL);
    if (ret != 0) {
        fprintf(stderr, "Error creating thread1: %d\n", ret);
        // 实际应用中可能需要销毁键
        pthread_key_delete(key);
        exit(EXIT_FAILURE);
    }

    // 创建第二个线程
    ret = pthread_create(&thread2, NULL, thread_function, NULL);
    if (ret != 0) {
        fprintf(stderr, "Error creating thread2: %d\n", ret);
        // 实际应用中需要等待 thread1 完成，并销毁键
        pthread_join(thread1, NULL); // 确保 thread1 完成，以便析构函数运行
        pthread_key_delete(key);
        exit(EXIT_FAILURE);
    }

    // 等待两个线程完成
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    // 销毁线程特定数据键
    // 注意：销毁键不会触发析构函数，析构函数只在线程退出时触发
    if (pthread_key_delete(key) != 0) {
        perror("pthread_key_delete");
        exit(EXIT_FAILURE);
    }

    printf("Main thread: All threads finished and TSD key deleted.\n");

    return 0; // 主线程正常退出
}