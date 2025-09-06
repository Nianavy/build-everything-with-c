// threadpool.c

#include "threadpool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  // For strerror
#include <stdarg.h>  // For va_list
#include <unistd.h>  // For sleep (如果内部辅助函数需要)

// --- 内部辅助函数声明 (仅在 .c 文件中可见) ---
static void *threadpool_worker(void *threadpool);

// 辅助错误打印函数 (不退出，且在库中应避免直接打印)
// 为了演示，这里保留一个可以关闭的宏定义。
// 真正的库应该提供一个注册日志回调的机制。
// #define THREADPOOL_ENABLE_LOGGING // 定义此宏以启用日志
#ifdef THREADPOOL_ENABLE_LOGGING
static void threadpool_log_error_internal(const char *file, int line, const char *func, const char *fmt, ...) {
    va_list args;
    fprintf(stderr, "ThreadPool Error [%s:%d in %s]: ", file, line, func);
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}
#define THREADPOOL_LOG_ERROR(fmt, ...) \
    threadpool_log_error_internal(__FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#else
#define THREADPOOL_LOG_ERROR(fmt, ...) do {} while(0) // 禁用日志输出
#endif


// --- 线程池初始化 ---
int threadpool_init(threadpool_t *pool, int thread_count, int queue_size) {
    if (pool == NULL || thread_count <= 0 || queue_size <= 0) {
        THREADPOOL_LOG_ERROR("Invalid parameters for threadpool_init.");
        return -1;
    }

    // 初始化所有资源状态为未分配/未初始化
    memset(pool, 0, sizeof(threadpool_t)); // 清零，所有指针为 NULL，bool为false

    pool->thread_count = thread_count;
    pool->queue_size = queue_size;
    pool->stop = false;

    // 分配线程数组
    pool->threads = (pthread_t*)malloc(sizeof(pthread_t) * thread_count);
    if (pool->threads == NULL) {
        THREADPOOL_LOG_ERROR("Failed to allocate memory for threads.");
        return -1; // 无需清理，因为 nothing else was allocated yet
    }
    // malloc 可能返回一些垃圾，初始化为 0 是个好习惯
    memset(pool->threads, 0, sizeof(pthread_t) * thread_count);

    // 分配任务队列
    pool->task_queue = (task_t*)malloc(sizeof(task_t) * queue_size);
    if (pool->task_queue == NULL) {
        THREADPOOL_LOG_ERROR("Failed to allocate memory for task queue.");
        goto err_threads_free;
    }

    // 初始化互斥量和条件变量
    int ret;
    ret = pthread_mutex_init(&(pool->lock), NULL);
    if (ret != 0) { THREADPOOL_LOG_ERROR("pthread_mutex_init failed: %s", strerror(ret)); goto err_task_queue_free; }
    pool->mutex_initialized = true;

    ret = pthread_cond_init(&(pool->notify_worker), NULL);
    if (ret != 0) { THREADPOOL_LOG_ERROR("pthread_cond_init (notify_worker) failed: %s", strerror(ret)); goto err_mutex_destroy; }
    pool->cond_worker_initialized = true;

    ret = pthread_cond_init(&(pool->notify_producer), NULL);
    if (ret != 0) { THREADPOOL_LOG_ERROR("pthread_cond_init (notify_producer) failed: %s", strerror(ret)); goto err_cond_worker_destroy; }
    pool->cond_producer_initialized = true;

    ret = pthread_cond_init(&(pool->notify_all_done), NULL);
    if (ret != 0) { THREADPOOL_LOG_ERROR("pthread_cond_init (notify_all_done) failed: %s", strerror(ret)); goto err_cond_producer_destroy; }
    pool->cond_all_done_initialized = true;

    // 创建工作线程
    // 记录实际成功创建的线程数，以便在销毁时知道需要 join 多少
    int i;
    for (i = 0; i < thread_count; ++i) {
        ret = pthread_create(&(pool->threads[i]), NULL, threadpool_worker, (void*)pool);
        if (ret != 0) {
            THREADPOOL_LOG_ERROR("pthread_create failed for thread %d: %s", i, strerror(ret));
            // 线程创建失败，就此停止初始化。
            // 已经创建的线程会在 destroy 中被 join。
            break; // 跳出循环，i 存储了成功创建的线程数
        }
    }
    pool->thread_count = i; // 更新实际线程数

    if (pool->thread_count < thread_count) {
        // 如果没有创建出所有期望的线程，则视为初始化失败
        THREADPOOL_LOG_ERROR("Only %d out of %d threads created. Initialisation failed.", pool->thread_count, thread_count);
        // 调用销毁函数清理已初始化的部分
        threadpool_destroy(pool);
        return -1;
    }

    return 0;

// 错误处理标签 (GoTo 语句是 C 语言中管理资源释放的常见做法)
// 这些标签用于回滚初始化失败的资源。
err_cond_producer_destroy:
    if (pool->cond_producer_initialized) pthread_cond_destroy(&(pool->notify_producer));
err_cond_worker_destroy:
    if (pool->cond_worker_initialized) pthread_cond_destroy(&(pool->notify_worker));
err_mutex_destroy:
    if (pool->mutex_initialized) pthread_mutex_destroy(&(pool->lock));
err_task_queue_free:
    free(pool->task_queue);
    pool->task_queue = NULL; // 清零指针，避免野指针
err_threads_free:
    free(pool->threads);
    pool->threads = NULL; // 清零指针，避免野指针
    return -1;
}

// --- 线程池添加任务实现 ---
int threadpool_add_task(threadpool_t *pool, void (*function)(void*), void *arg) {
    if (pool == NULL || function == NULL) { // arg可以为NULL
        THREADPOOL_LOG_ERROR("Invalid parameters for threadpool_add_task.");
        return -1;
    }
    // 检查线程池是否有效，例如检查核心资源指针是否为 NULL
    if (pool->threads == NULL || pool->task_queue == NULL || !pool->mutex_initialized) {
        THREADPOOL_LOG_ERROR("Attempted to add task to an uninitialized or invalid thread pool.");
        return -1;
    }


    int ret = pthread_mutex_lock(&(pool->lock));
    if (ret != 0) { THREADPOOL_LOG_ERROR("threadpool_add_task: pthread_mutex_lock failed: %s", strerror(ret)); return -1; }

    // 等待队列有空闲空间
    while (pool->queued_tasks == pool->queue_size && !pool->stop) {
        ret = pthread_cond_wait(&(pool->notify_producer), &(pool->lock));
        if (ret != 0) { THREADPOOL_LOG_ERROR("threadpool_add_task: pthread_cond_wait failed: %s", strerror(ret)); goto cleanup_unlock; }
    }

    // 如果线程池正在停止，则拒绝新任务
    if (pool->stop) {
        THREADPOOL_LOG_ERROR("Thread pool is stopping, cannot add new tasks.");
        goto cleanup_unlock;
    }

    // 将任务添加到队列
    int next_back = (pool->queue_back + 1) % pool->queue_size;
    pool->task_queue[pool->queue_back].function = function;
    pool->task_queue[pool->queue_back].arg = arg;
    pool->queue_back = next_back;
    pool->queued_tasks++;

    // 通知一个工作线程有新任务
    ret = pthread_cond_signal(&(pool->notify_worker));
    if (ret != 0) { THREADPOOL_LOG_ERROR("threadpool_add_task: pthread_cond_signal failed: %s", strerror(ret)); goto cleanup_unlock; }

    ret = pthread_mutex_unlock(&(pool->lock));
    return (ret == 0) ? 0 : -1;

cleanup_unlock:
    pthread_mutex_unlock(&(pool->lock));
    return -1;
}

// --- 线程池工作线程函数 (消费者，内部函数) ---
static void *threadpool_worker(void *threadpool) {
    threadpool_t *pool = (threadpool_t*)threadpool;
    task_t task;
    int ret;

    while (true) {
        ret = pthread_mutex_lock(&(pool->lock));
        if (ret != 0) { THREADPOOL_LOG_ERROR("Worker: pthread_mutex_lock failed: %s", strerror(ret)); break; }

        while (pool->queued_tasks == 0 && !pool->stop) {
            ret = pthread_cond_wait(&(pool->notify_worker), &(pool->lock));
            if (ret != 0) { THREADPOOL_LOG_ERROR("Worker: pthread_cond_wait failed: %s", strerror(ret)); goto cleanup_unlock_worker; }
        }

        // 退出条件：如果线程池停止且队列中已没有任务
        if (pool->stop && pool->queued_tasks == 0) {
            // 在退出前，广播唤醒所有其他可能仍在等待的 worker，让他们也能退出
            pthread_cond_broadcast(&(pool->notify_worker));
            goto cleanup_unlock_worker;
        }

        // 从任务队列中取出任务
        task = pool->task_queue[pool->queue_front];
        pool->queue_front = (pool->queue_front + 1) % pool->queue_size;
        pool->queued_tasks--;

        pool->tasks_in_progress++; // 任务开始执行

        // 通知生产者：队列现在有空闲位置了
        ret = pthread_cond_signal(&(pool->notify_producer));
        if (ret != 0) { THREADPOOL_LOG_ERROR("Worker: pthread_cond_signal (notify_producer) failed: %s", strerror(ret)); } // 非致命错误，继续执行任务

        ret = pthread_mutex_unlock(&(pool->lock));
        if (ret != 0) { THREADPOOL_LOG_ERROR("Worker: pthread_mutex_unlock failed: %s", strerror(ret)); } // 非致命错误，继续执行任务

        // 执行任务 (在锁外执行，避免阻塞其他线程)
        if (task.function) {
            (*(task.function))(task.arg);
        }

        // 任务执行完毕，更新活跃任务计数器
        ret = pthread_mutex_lock(&(pool->lock));
        if (ret != 0) { THREADPOOL_LOG_ERROR("Worker: pthread_mutex_lock failed (after task): %s", strerror(ret)); break; }

        pool->tasks_in_progress--; // 任务完成

        // 如果所有任务 (队列和执行中的) 都已完成，且线程池已停止，通知销毁者
        if (pool->tasks_in_progress == 0 && pool->queued_tasks == 0 && pool->stop) {
            ret = pthread_cond_signal(&(pool->notify_all_done));
            if (ret != 0) { THREADPOOL_LOG_ERROR("Worker: pthread_cond_signal (notify_all_done) failed: %s", strerror(ret)); }
        }

        ret = pthread_mutex_unlock(&(pool->lock));
        if (ret != 0) { THREADPOOL_LOG_ERROR("Worker: pthread_mutex_unlock failed (after task): %s", strerror(ret)); }
    }

cleanup_unlock_worker:
    pthread_mutex_unlock(&(pool->lock)); // 确保退出前解锁
    pthread_exit(NULL); // 线程正常退出，不影响主进程
}

// --- 线程池销毁实现 ---
int threadpool_destroy(threadpool_t *pool) {
    if (pool == NULL) {
        THREADPOOL_LOG_ERROR("Invalid pool pointer for threadpool_destroy.");
        return -1;
    }

    // 检查是否已经初始化过，如果主要资源为 NULL，则假定没有完全初始化或已经销毁
    // 这种检查可以避免对未初始化的互斥量/条件变量进行销毁，导致未定义行为。
    if (pool->threads == NULL && pool->task_queue == NULL &&
        !pool->mutex_initialized && !pool->cond_worker_initialized &&
        !pool->cond_producer_initialized && !pool->cond_all_done_initialized) {
        THREADPOOL_LOG_ERROR("Attempted to destroy an uninitialized or already destroyed thread pool.");
        // 如果所有关键资源都是未初始化的，我们可以安全地认为它已被销毁或从未成功初始化。
        // 返回0表示清理完毕（即使没什么可清理的）
        return 0;
    }

    int ret = pthread_mutex_lock(&(pool->lock));
    if (ret != 0) { THREADPOOL_LOG_ERROR("threadpool_destroy: pthread_mutex_lock failed: %s", strerror(ret)); return -1; }

    pool->stop = true; // 设置停止标志

    // 唤醒所有工作线程，让它们检查 stop 标志，然后退出
    ret = pthread_cond_broadcast(&(pool->notify_worker));
    if (ret != 0) { THREADPOOL_LOG_ERROR("threadpool_destroy: pthread_cond_broadcast (notify_worker) failed: %s", strerror(ret)); goto cleanup_unlock; }

    // 同时唤醒所有等待的生产者，让他们知道池已停止，无法再添加任务
    ret = pthread_cond_broadcast(&(pool->notify_producer));
    if (ret != 0) { THREADPOOL_LOG_ERROR("threadpool_destroy: pthread_cond_broadcast (notify_producer) failed: %s", strerror(ret)); goto cleanup_unlock; }

    // 等待所有任务完成 (队列中 + 正在执行的)。
    // 如果没有任务，立即跳过等待。
    while (pool->queued_tasks > 0 || pool->tasks_in_progress > 0) {
        ret = pthread_cond_wait(&(pool->notify_all_done), &(pool->lock));
        if (ret != 0) { THREADPOOL_LOG_ERROR("threadpool_destroy: pthread_cond_wait (notify_all_done) failed: %s", strerror(ret)); goto cleanup_unlock; }
    }

    ret = pthread_mutex_unlock(&(pool->lock));
    if (ret != 0) { THREADPOOL_LOG_ERROR("threadpool_destroy: pthread_mutex_unlock failed: %s", strerror(ret)); return -1; }

    // 等待所有工作线程退出
    for (int i = 0; i < pool->thread_count; ++i) {
        ret = pthread_join(pool->threads[i], NULL);
        if (ret != 0) { THREADPOOL_LOG_ERROR("threadpool_destroy: pthread_join failed for thread %d: %s", i, strerror(ret)); }
    }

    // 销毁互斥量和条件变量 (根据标志安全销毁)
    if (pool->mutex_initialized) pthread_mutex_destroy(&(pool->lock));
    if (pool->cond_worker_initialized) pthread_cond_destroy(&(pool->notify_worker));
    if (pool->cond_producer_initialized) pthread_cond_destroy(&(pool->notify_producer));
    if (pool->cond_all_done_initialized) pthread_cond_destroy(&(pool->notify_all_done));

    // 释放动态分配的内存 (根据指针是否为 NULL 安全释放)
    free(pool->threads);
    free(pool->task_queue);

    // 清零结构体，以便可以安全地重新初始化或避免误操作
    memset(pool, 0, sizeof(threadpool_t));

    return 0;

cleanup_unlock:
    pthread_mutex_unlock(&(pool->lock));
    return -1;
}