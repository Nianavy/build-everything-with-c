#include <fmt/core.h>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

template <typename T>
class LockedQueue {
    std::mutex mtx_;
    std::condition_variable cv_;
    std::queue<T> queue_;
    std::atomic_size_t size_{0};
    std::atomic_bool closed_{false};

   public:
    explicit LockedQueue() = default;
    ~LockedQueue() {
        if (!empty()) cv_.notify_all();
        close();
    }

    bool closed() { return closed_.load(std::memory_order_relaxed); }

    void close() {
        if (!closed()) {
            closed_.store(true, std::memory_order_relaxed);
            cv_.notify_all();
        }
    }

    std::size_t size() { return size_.load(std::memory_order_relaxed); }

    bool empty() {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.empty();
    }

    void push(T val) {
        if (closed()) return;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            queue_.push(std::move(val));
        }
        size_.fetch_add(1, std::memory_order_relaxed);
        cv_.notify_one();
    }

    bool pop(T &val) {
        std::unique_lock<std::mutex> lock{mtx_};
        cv_.wait(lock, [this]() { return !queue_.empty() || closed(); });
        if (queue_.empty()) return false;
        val = std::move(queue_.front());
        queue_.pop();
        size_.fetch_sub(1, std::memory_order_relaxed);
        return true;
    }

    bool try_pop(T &val) {
        if (queue_.empty()) return false;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            val = std::move(queue_.front());
            queue_.pop();
            size_.fetch_sub(1, std::memory_order_relaxed);
            return true;
        }
    }
};

class ThreadPool {
    static inline std::shared_ptr<ThreadPool> instance_{nullptr};
    static inline std::once_flag flag_;
    LockedQueue<std::function<void()>> tasks_;
    std::vector<std::thread> threads_;

    ThreadPool(std::size_t thread_num) {
        for (std::size_t i = 0; i < thread_num; ++i) {
            threads_.emplace_back([this]() -> void { work(); });
        }
    }

    void work() {
        std::function<void()> task;
        while (tasks_.pop(task)) task();
    }

   public:
    ~ThreadPool() {
        close();
        for (auto &t : threads_) {
            if (t.joinable()) t.join();
        }
    }

    static std::shared_ptr<ThreadPool> getInstance(
        std::size_t thread_num = std::thread::hardware_concurrency()) {
        std::call_once(flag_, [&]() -> void {
            instance_ = std::shared_ptr<ThreadPool>(new ThreadPool(thread_num));
        });
        return instance_;
    }

    void close() { tasks_.close(); }

    bool closed() { return tasks_.closed(); }

    template <typename F, typename... Args>
    std::future<std::invoke_result_t<F, Args...>> addTask(F &&f,
                                                          Args &&...args) {
        using return_type = std::invoke_result_t<F, Args...>;
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        auto res = task->get_future();
        tasks_.push([task]() { (*task)(); });
        return res;
    }
};

int main() {
    auto pool = ThreadPool::getInstance();

    std::vector<std::future<int>> tasks;

    for (int i = 0; i < 10; ++i) {
        tasks.emplace_back(pool->addTask([i]() { return i; }));
    }

    for (auto &t : tasks) fmt::print("get res : {}\n", t.get());

    return 0;
}