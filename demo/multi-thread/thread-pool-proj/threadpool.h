// threadpool.h

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>
#include <stdbool.h>

// --- 配置参数 (对外可见的配置) ---
#define THREADS_MAX_DEFAULT 8          // 默认线程池中最大线程数量
#define QUEUE_SIZE_MAX_DEFAULT 100     // 默认任务队列最大容量

// --- 任务结构 ---
typedef struct {
    void (*function)(void *arg); // 任务函数指针
    void *arg;                   // 任务函数参数
    // 注意：arg 的内存生命周期由任务的提交者负责管理！线程池不负责释放 arg。
} task_t;

// --- 线程池结构 (完整定义，现在在 .h 文件中可见) ---
typedef struct threadpool_t {
    pthread_mutex_t lock;             // 保护线程池结构的互斥量
    pthread_cond_t notify_worker;     // 条件变量：通知工作线程队列有任务
    pthread_cond_t notify_producer;   // 条件变量：通知生产者队列有空闲
    pthread_cond_t notify_all_done;   // 条件变量：通知销毁者所有任务已完成

    pthread_t *threads;               // 工作线程 ID 数组
    task_t *task_queue;               // 任务队列 (环形缓冲区)

    int thread_count;                 // 线程池中线程的实际数量
    int queue_size;                   // 任务队列的实际容量

    int queued_tasks;                 // 队列中当前等待的任务数量
    int tasks_in_progress;            // 正在处理或已排队但尚未完成的任务总数 (用于优雅关闭)
    int queue_front;                  // 队列头部索引 (消费者从这里取)
    int queue_back;                   // 队列尾部索引 (生产者从这里放)

    bool stop;                        // 停止标志，指示线程池是否正在关闭
    bool init_failed;                 // 标记初始化是否失败，防止重复销毁或操作未完全初始化的池

    /*
        用于在初始化和销毁过程中追踪 pthread 原语是否已成功初始化。
        在 `memset(pool, 0, ...)` 后，这些标志默认为 `false`，
        在对应原语初始化成功后设置为 `true`，确保安全销毁。
    */
    bool mutex_initialized;
    bool cond_worker_initialized;
    bool cond_producer_initialized;
    bool cond_all_done_initialized;

} threadpool_t;

// --- 线程池公共接口函数声明 ---

/**
 * @brief 初始化线程池。
 * @param pool 指向 threadpool_t 结构的指针。
 * @param num_threads 线程池中要创建的线程数量。
 * @param q_size 任务队列的最大容量。
 * @return 0 成功，-1 失败。
 */
int threadpool_init(threadpool_t *pool, int num_threads, int q_size);

/**
 * @brief 添加一个任务到线程池。
 *        如果队列已满，生产者线程将阻塞直到有空间可用。
 *        注意：任务参数 `arg` 的内存生命周期由调用者负责！如果添加失败，调用者必须自行释放 `arg`。
 * @param pool 指向 threadpool_t 结构的指针。
 * @param function 任务函数指针。
 * @param arg 任务函数的参数指针。
 * @return 0 成功，-1 失败。
 */
int threadpool_add_task(threadpool_t *pool, void (*function)(void*), void *arg);

/**
 * @brief 销毁线程池，并等待所有已提交任务完成及工作线程退出。
 * @param pool 指向 threadpool_t 结构的指针。
 * @return 0 成功，-1 失败。
 */
int threadpool_destroy(threadpool_t *pool);

#endif // THREADPOOL_H