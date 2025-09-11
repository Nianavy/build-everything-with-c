/*
demo for semaphore
*/

#include <pthread.h>    // POSIX 线程库，用于创建和管理线程
#include <semaphore.h>  // POSIX 信号量库，用于信号量操作
#include <stdio.h>      // 用于 printf()
#include <unistd.h>     // 用于 sleep() 函数

/*
    mutex -> mutual exclusion (access)  //
   互斥量：用于互斥访问共享资源（同一时间只有一个线程能访问） semaphore ->
   multiple access (signaling) // 信号量：可以允许多个访问，或用于事件通知
*/

sem_t sem;  // 声明一个全局信号量变量

void *signal_event() {                // 这是线程 t2 将执行的函数
    printf("Doing some workd...\n");  // 打印提示信息
    sleep(2);  // 模拟耗时操作，线程会在这里暂停执行 2 秒
    printf("Signaling event completion!\n");  // 打印提示信息
    sem_post(&sem);  // **V 操作：将信号量 sem 的值加 1。如果 sem=0，现在就变成
                     // 1。如果此时有线程在 sem_wait 处阻塞，它将被唤醒。**
    return NULL;  // 线程函数必须返回 NULL 或一个指针
}

void *wait_for_event() {               // 这是线程 t1 将执行的函数
    printf("Waiting for event...\n");  // 打印提示信息
    sem_wait(&sem);  // **P 操作：尝试将信号量 sem 的值减 1。**
                     // **由于在 main 函数中 sem 初始化为 0，当 t1 第一次调用
                     // sem_wait 时，它发现 sem 的值不是 >0 的，因此 t1
                     // 线程会被立即阻塞在这里，直到 sem 的值被其他线程通过
                     // sem_post 增加。**
    printf("Event has been triggered!\n");  // 当 t1 被唤醒并成功将 sem 减 1
                                            // 后，它会继续执行这里
    return NULL;  // 线程函数必须返回 NULL 或一个指针
}

int main() {           // 主线程函数
    pthread_t t1, t2;  // 声明两个线程 ID 变量

    // **信号量初始化：关键点在这里！**
    // sem_init(sem_t *sem, int pshared, unsigned int value);
    // 第一个参数: &sem 是要初始化的信号量变量的地址。
    // 第二个参数: 0 表示这个信号量只在当前进程内的线程间共享 (非进程间共享)。
    // 第三个参数: 0 表示信号量的初始值为 0。
    // **这意味着在任何线程调用 sem_post 之前，信号量都是“不可用”的状态。**
    sem_init(&sem, 0, 0);

    // 创建线程 t1，让它执行 wait_for_event 函数
    pthread_create(&t1, NULL, wait_for_event, NULL);
    // 创建线程 t2，让它执行 signal_event 函数
    pthread_create(&t2, NULL, signal_event, NULL);

    // 主线程等待 t1 线程结束。
    // 由于 t1 会被 sem_wait 阻塞，主线程会一直等到 t1 被唤醒并完成任务。
    pthread_join(t1, NULL);
    // 主线程等待 t2 线程结束。
    pthread_join(t2, NULL);

    // 销毁信号量，释放其占用的资源。
    sem_destroy(&sem);
    return 0;  // 主线程正常退出
}