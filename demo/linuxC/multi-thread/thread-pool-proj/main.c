// main.c (几乎不变，只是适应了错误返回)

#include <pthread.h>  // For pthread_self() in example_task
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>  // For sleep

#include "threadpool.h"  // 包含线程池的公共接口

// --- 示例任务 ---
// 注意：这个任务是线程池的使用者定义的，它的参数释放由它自己负责
#define EXAMPLE_TASKS_COUNT_MAIN 200  // 演示中提交的任务数量
#define TASK_SLEEP_TIME_MAIN 1        // 示例任务的延迟时间 (秒)

void example_task(void *arg) {
    int *num = (int *)arg;
    // 打印当前线程 ID 以区分是哪个工作线程在处理任务
    printf("Thread %lu: Processing task %d\n", (unsigned long)pthread_self(),
           *num);
    sleep(TASK_SLEEP_TIME_MAIN);  // 模拟任务执行时间
    free(arg);  // 任务完成，释放任务参数内存 (由任务的定义者负责)
}

// --- 主函数 ---
int main() {
    threadpool_t pool;  // 线程池实例
    int ret;

    printf("Main: Initializing thread pool...\n");
    // 初始化线程池
    ret = threadpool_init(&pool, THREADS_MAX_DEFAULT, QUEUE_SIZE_MAX_DEFAULT);
    if (ret != 0) {
        fprintf(stderr, "Main: Failed to initialize thread pool. Exiting.\n");
        return 1;
    }

    printf("Main: Adding %d tasks...\n", EXAMPLE_TASKS_COUNT_MAIN);
    // 添加任务
    for (int i = 0; i < EXAMPLE_TASKS_COUNT_MAIN; ++i) {
        int *task_num = (int *)malloc(sizeof(int));
        if (task_num == NULL) {
            perror("malloc for task_num failed");
            // 简单处理：跳过当前任务，继续尝试添加下一个
            continue;
        }
        *task_num = i;
        ret = threadpool_add_task(&pool, example_task, task_num);
        if (ret != 0) {
            fprintf(stderr,
                    "Main: Failed to add task %d. Self-releasing arg.\n", i);
            free(task_num);  // threadpool_add_task 失败时，arg 由调用者释放
        }
    }
    printf(
        "Main: All tasks submitted. Waiting for completion and destroying "
        "pool...\n");

    // 销毁线程池 (会自动等待所有已提交任务完成)
    ret = threadpool_destroy(&pool);
    if (ret != 0) {
        fprintf(stderr, "Main: Failed to destroy thread pool. Exiting.\n");
        return 1;
    }

    printf("Main: Program finished successfully.\n");
    return 0;
}