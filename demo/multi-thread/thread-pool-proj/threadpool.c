// threadpool.c

#include "threadpool.h" 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  // For strerror
#include <stdarg.h>  // For va_list
#include <unistd.h>  // For sleep (如果内部辅助函数需要)


// --- 内部辅助函数声明 (仅在 .c 文件中可见) ---
static void handle_pthread_error_internal(int err_code, const char *file, int line, const char *func, const char *fmt, ...) __attribute__((noreturn));
static void *threadpool_worker(void *threadpool);

// 宏定义一个更方便的错误处理调用，自动传入文件、行号和函数名
#define HANDLE_PTHREAD_ERROR(err_code, fmt, ...) \
    handle_pthread_error_internal(err_code, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

// --- 内部辅助函数定义：错误处理 ---
static void handle_pthread_error_internal(int err_code, const char *file, int line, const char *func, const char *fmt, ...) {
    va_list args;
    fprintf(stderr, "Error in %s at %s:%d: ", func, file, line);
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, " (pthread error code %d: %s)\n", err_code, strerror(err_code)); 
    exit(EXIT_FAILURE); 
}

// --- 线程池初始化 ---
int threadpool_init(threadpool_t *pool, int thread_count, int queue_size) {
    if (pool == NULL || thread_count <= 0 || queue_size <= 0) {
        fprintf(stderr, "Error: Invalid parameters for threadpool_init.\n");
        return -1;
    }

    pool->thread_count = thread_count;
    pool->queue_size = queue_size;
    pool->queue_front = 0;
    pool->queue_back = 0;
    pool->queued_tasks = 0;
    pool->tasks_in_progress = 0;
    pool->stop = false;

    // 分配线程数组
    pool->threads = (pthread_t*)malloc(sizeof(pthread_t) * thread_count);
    if (pool->threads == NULL) {
        fprintf(stderr, "Error: Failed to allocate memory for threads.\n");
        return -1;
    }

    // 分配任务队列
    pool->task_queue = (task_t*)malloc(sizeof(task_t) * queue_size);
    if (pool->task_queue == NULL) {
        fprintf(stderr, "Error: Failed to allocate memory for task queue.\n");
        free(pool->threads);
        return -1;
    }

    // 初始化互斥量和条件变量
    int ret = pthread_mutex_init(&(pool->lock), NULL);
    if (ret != 0) HANDLE_PTHREAD_ERROR(ret, "pthread_mutex_init failed.");

    ret = pthread_cond_init(&(pool->notify_worker), NULL);
    if (ret != 0) HANDLE_PTHREAD_ERROR(ret, "pthread_cond_init (notify_worker) failed.");

    ret = pthread_cond_init(&(pool->notify_producer), NULL);
    if (ret != 0) HANDLE_PTHREAD_ERROR(ret, "pthread_cond_init (notify_producer) failed.");

    ret = pthread_cond_init(&(pool->notify_all_done), NULL);
    if (ret != 0) HANDLE_PTHREAD_ERROR(ret, "pthread_cond_init (notify_all_done) failed.");

    // 创建工作线程
    for (int i = 0; i < thread_count; ++i) {
        ret = pthread_create(&(pool->threads[i]), NULL, threadpool_worker, (void*)pool);
        if (ret != 0) HANDLE_PTHREAD_ERROR(ret, "pthread_create failed for thread %d.", i);
    }

    printf("Thread pool initialized with %d threads and queue size %d.\n", thread_count, queue_size);
    return 0;
}

// --- 线程池添加任务实现 ---
int threadpool_add_task(threadpool_t *pool, void (*function)(void*), void *arg) {
    if (pool == NULL || function == NULL || arg == NULL) {
        fprintf(stderr, "Error: Invalid parameters for threadpool_add_task.\n");
        return -1;
    }

    int ret = pthread_mutex_lock(&(pool->lock));
    if (ret != 0) HANDLE_PTHREAD_ERROR(ret, "threadpool_add_task: pthread_mutex_lock failed.");

    // 等待队列有空闲空间
    while (pool->queued_tasks == pool->queue_size && !pool->stop) {
        printf("Warning: Task queue is full. Producer waiting for space...\n");
        ret = pthread_cond_wait(&(pool->notify_producer), &(pool->lock));
        if (ret != 0) HANDLE_PTHREAD_ERROR(ret, "threadpool_add_task: pthread_cond_wait failed.");
    }

    // 如果线程池正在停止，则拒绝新任务
    if (pool->stop) {
        ret = pthread_mutex_unlock(&(pool->lock));
        if (ret != 0) HANDLE_PTHREAD_ERROR(ret, "threadpool_add_task: pthread_mutex_unlock failed (on stop).");
        free(arg); // 释放任务参数
        return -1;
    }

    // 将任务添加到队列
    int next_back = (pool->queue_back + 1) % pool->queue_size;
    pool->task_queue[pool->queue_back].function = function;
    pool->task_queue[pool->queue_back].arg = arg;
    pool->queue_back = next_back;
    pool->queued_tasks++;

    // 通知一个工作线程有新任务
    ret = pthread_cond_signal(&(pool->notify_worker));
    if (ret != 0) HANDLE_PTHREAD_ERROR(ret, "threadpool_add_task: pthread_cond_signal failed.");

    ret = pthread_mutex_unlock(&(pool->lock));
    if (ret != 0) HANDLE_PTHREAD_ERROR(ret, "threadpool_add_task: pthread_mutex_unlock failed.");

    return 0;
}

// --- 线程池工作线程函数 (消费者，内部函数) ---
static void *threadpool_worker(void *threadpool) {
    threadpool_t *pool = (threadpool_t*)threadpool;
    task_t task;
    int ret;

    while (!pool->stop || pool->queued_tasks > 0) {
        ret = pthread_mutex_lock(&(pool->lock));
        if (ret != 0) HANDLE_PTHREAD_ERROR(ret, "Worker: pthread_mutex_lock failed.");

        while (pool->queued_tasks == 0 && !pool->stop) {
            ret = pthread_cond_wait(&(pool->notify_worker), &(pool->lock));
            if (ret != 0) HANDLE_PTHREAD_ERROR(ret, "Worker: pthread_cond_wait failed.");
        }

        if (pool->stop && pool->queued_tasks == 0) {
            break;
        }

        // 从任务队列中取出任务
        task = pool->task_queue[pool->queue_front];
        pool->queue_front = (pool->queue_front + 1) % pool->queue_size;
        pool->queued_tasks--;

        // 在取出任务后，增加 tasks_in_progress
        pool->tasks_in_progress++;

        // 通知生产者：队列现在有空闲位置了
        ret = pthread_cond_signal(&(pool->notify_producer));
        if (ret != 0) HANDLE_PTHREAD_ERROR(ret, "Worker: pthread_cond_signal (notify_producer) failed.");

        ret = pthread_mutex_unlock(&(pool->lock));
        if (ret != 0) HANDLE_PTHREAD_ERROR(ret, "Worker: pthread_mutex_unlock failed.");

        // 执行任务
        (*(task.function))(task.arg);

        // 任务执行完毕，更新活跃任务计数器
        ret = pthread_mutex_lock(&(pool->lock));
        if (ret != 0) HANDLE_PTHREAD_ERROR(ret, "Worker: pthread_mutex_lock failed (after task).");

        pool->tasks_in_progress--;

        // 如果所有任务都已完成，通知销毁者
        if (pool->tasks_in_progress == 0) {
            ret = pthread_cond_signal(&(pool->notify_all_done));
            if (ret != 0) HANDLE_PTHREAD_ERROR(ret, "Worker: pthread_cond_signal (notify_all_done) failed.");
        }

        ret = pthread_mutex_unlock(&(pool->lock));
        if (ret != 0) HANDLE_PTHREAD_ERROR(ret, "Worker: pthread_mutex_unlock failed (after task).");
    }

    // 线程退出前广播，确保其他工作线程也能退出
    ret = pthread_cond_broadcast(&(pool->notify_worker));
    if (ret != 0) HANDLE_PTHREAD_ERROR(ret, "Worker: pthread_cond_broadcast (on exit) failed.");

    pthread_exit(NULL);
}

// --- 线程池销毁实现 ---
int threadpool_destroy(threadpool_t *pool) {
    if (pool == NULL) {
        fprintf(stderr, "Error: Invalid pool pointer for threadpool_destroy.\n");
        return -1;
    }

    int ret = pthread_mutex_lock(&(pool->lock));
    if (ret != 0) HANDLE_PTHREAD_ERROR(ret, "threadpool_destroy: pthread_mutex_lock failed.");

    pool->stop = true; // 设置停止标志

    // 唤醒所有工作线程，让它们检查 stop 标志
    ret = pthread_cond_broadcast(&(pool->notify_worker));
    if (ret != 0) HANDLE_PTHREAD_ERROR(ret, "threadpool_destroy: pthread_cond_broadcast failed.");

    ret = pthread_mutex_unlock(&(pool->lock));
    if (ret != 0) HANDLE_PTHREAD_ERROR(ret, "threadpool_destroy: pthread_mutex_unlock failed.");

    // 等待所有工作线程退出
    for (int i = 0; i < pool->thread_count; ++i) {
        ret = pthread_join(pool->threads[i], NULL);
        if (ret != 0) HANDLE_PTHREAD_ERROR(ret, "threadpool_destroy: pthread_join failed for thread %d.", i);
    }

    // 销毁互斥量和条件变量
    pthread_mutex_destroy(&(pool->lock));
    pthread_cond_destroy(&(pool->notify_worker));
    pthread_cond_destroy(&(pool->notify_producer));
    pthread_cond_destroy(&(pool->notify_all_done));

    // 释放动态分配的内存
    free(pool->threads);
    free(pool->task_queue);

    return 0;
}