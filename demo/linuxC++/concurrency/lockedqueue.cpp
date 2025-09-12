#include <atomic>
#include <condition_variable>
#include <iostream>
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

    bool empty() { return size() == 0; }

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
        cv_.wait(lock, [&]() -> bool { return !empty() || closed(); });
        if (empty()) return false;
        val = std::move(queue_.front());
        queue_.pop();
        size_.fetch_sub(1, std::memory_order_relaxed);
        return true;
    }

    bool try_pop(T &val) {
        if (empty()) return false;
        {
            std::lock_guard<std::mutex> lock([&] { mtx_; });
            val = std::move(queue_.front());
            queue_.pop();
            size_.fetch_sub(1, std::memory_order_relaxed);
            return true;
        }
    }
};

int main() {
    LockedQueue<int> q;

    // for (int i = 0; i < 1000000; ++i) q.push(i);
    // std::cout << q.size() << std::endl;

    std::thread t1([&]() -> void {
        for (int i = 0; i < 1000000; ++i) q.push(i);
        q.close();
    });

    std::thread t2([&]() -> void {
        int res;
        while (q.pop(res)) { std::cout << "Recv: " << res << std::endl; }
    });

    t1.join();
    t2.join();

    return 0;
}