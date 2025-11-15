// thread_pool.h
#ifndef THREAD_POOL_H
#define THREAD_POOL_H
#include <condition_variable>
#include <mutex>
#include <queue>
#include <functional>
#include <future>
#include <thread>
#include <utility>
#include <vector>

// SafeQueue实现
template <typename T>
class SafeQueue {
private:
    std::queue<T> queue_;
    std::mutex mutex_;
public:
    SafeQueue() {}
    SafeQueue(SafeQueue &&other) {}
    ~SafeQueue() {}
    bool empty() {
        std::unique_lock<std::mutex> lock(mutex_);
        return queue_.empty();
    }
    int size() {
        std::unique_lock<std::mutex> lock(mutex_);
        return queue_.size();
    }
    // 队列添加元素
    void enqueue(T &t) {
        std::unique_lock<std::mutex> lock(mutex_);
        queue_.emplace(t);
    }
    // 队列取出元素
    bool dequeue(T &t) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (queue_.empty()) return false;
        t = std::move(queue_.front());
        queue_.pop();
        return true;
    }
};

class ThreadPool {
private:
    SafeQueue<std::function<void()>> queue_;
    std::vector<std::thread> threads_;
    bool shutdown_;
    std::mutex conditional_mutex_;
    std::condition_variable conditional_lock_;

    class ThreadWorker {
    private:
        ThreadPool *pool_;
        int id_;
    public:
        ThreadWorker(ThreadPool *pool, const int id) : pool_(pool), id_(id) {}
        void operator()() {
            std::function<void()> func;
            bool dequeued;
            while (!pool_->shutdown_) {
                {
                    std::unique_lock<std::mutex> lock(pool_->conditional_mutex_);
                    if (pool_->queue_.empty()) {
                        pool_->conditional_lock_.wait(lock);
                    }
                    dequeued = pool_->queue_.dequeue(func);
                }
                if (dequeued) func();
            }
        }
    };
public:
    ThreadPool(const int n_threads = 4) : threads_(std::vector<std::thread>(n_threads)), shutdown_(false) {}
    ThreadPool(const ThreadPool &) = delete;
    ThreadPool(ThreadPool &&) = delete;
    ThreadPool& operator=(const ThreadPool &) = delete;
    ThreadPool& operator=(ThreadPool &&) = delete;
    void init() {
        for (std::size_t i = 0; i < threads_.size(); ++i) {
            threads_.at(i) = std::thread(ThreadWorker(this, static_cast<int>(i)));
        }
    }
    void shutdown() {
        shutdown_ = true;
        conditional_lock_.notify_all();
        for (std::size_t i = 0; i < threads_.size(); ++i) {
            if (threads_.at(i).joinable()) {
                threads_.at(i).join();
            }
        }
    }
    template <typename F, typename... Args>
    auto submit(F &&f, Args &&...args) -> std::future<decltype(f(args...))> {
        std::function<decltype(f(args...))()> func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
        auto task_ptr = std::make_shared<std::packaged_task<decltype(f(args...))()>>(func);
        std::function<void()> warpper_func = [task_ptr]() { (*task_ptr)(); };
        queue_.enqueue(warpper_func);
        conditional_lock_.notify_one();
        return task_ptr->get_future();
    }
};
#endif