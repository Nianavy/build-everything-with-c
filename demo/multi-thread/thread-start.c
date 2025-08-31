#include <stdio.h>
#include <pthread.h>
#include <unistd.h> // For sleep

#define THREAD_COUNT 10
int counter = 0;
pthread_mutex_t counter_lock = PTHREAD_MUTEX_INITIALIZER;

void *thread_target(void *arg) {
    long thread_id = (long)arg;
    printf("Thread %ld: I am a thread\n", thread_id);
    pthread_mutex_lock(&counter_lock);
    for (int i = 0; i < 1000000; ++i) ++counter;
    pthread_mutex_unlock(&counter_lock);
    printf("Counter is %d\n", counter);
    return (void*)thread_id; // 返回线程ID作为退出状态
}

int main() {
    pthread_t threads[THREAD_COUNT];
    int ret;

    for(long i = 0; i < THREAD_COUNT; ++i) {
        ret = pthread_create(&threads[i], NULL, thread_target, (void*)i);
        if (ret != 0) {
            fprintf(stderr, "Error creating thread %ld: %d\n", i, ret);
            // 实际项目中，这里需要清理之前已创建的线程
            return -1;
        }
    }

    printf("Main thread: All threads created. Waiting for them to finish...\n");

    for(long i = 0; i < THREAD_COUNT; ++i) {
        void *thread_ret_val;
        ret = pthread_join(threads[i], &thread_ret_val);
        if (ret != 0) {
            fprintf(stderr, "Error joining thread %ld: %d\n", i, ret);
        } else {
            printf("Main thread: Thread %ld finished with return value %ld\n", i, (long)thread_ret_val);
        }
    }

    printf("Main thread: All threads finished.\n");
    return 0;
}