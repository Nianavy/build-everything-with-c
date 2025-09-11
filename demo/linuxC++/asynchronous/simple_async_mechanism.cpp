#include <fmt/core.h>

#include <condition_variable>
#include <exception>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace my_async {

template <typename T>
class assoc_state {
    std::mutex mtx_;
    std::condition_variable cv_;
    T value_;
    bool ready_;

   public:
    assoc_state() : ready_(false) {}

    void set_value(T val) {
        if (ready_) return;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            value_ = val;
            ready_ = true;
        }
        cv_.notify_all();
    }

    T wait() {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [&]() { return ready_; });
        return value_;
    }
};

template <typename T>
class future {
    std::shared_ptr<assoc_state<T>> state_{nullptr};

   public:
    explicit future(std::shared_ptr<assoc_state<T>> state) : state_(state) {}

    T get() {
        if (!state_) throw std::runtime_error("state_ = nullptr");
        return state_->wait();
    }
};

template <typename T>
class promise {
    std::shared_ptr<assoc_state<T>> state_{nullptr};

   public:
    explicit promise() : state_(std::make_shared<assoc_state<T>>()) {}

    future<T> get_future() {
        if (!state_) throw std::runtime_error("state_ = nullptr");
        return future<T>(state_);
    }

    void set_value(T val) {
        if (!state_) throw std::runtime_error("state_ = nullptr");
        state_->set_value(val);
    }
};

};  // namespace my_async

int main() {
    my_async::promise<int> p;
    std::thread t1([&]() -> void {
        fmt::print("thread 1 start task...\n");
        std::this_thread::sleep_for(std::chrono::seconds(1));
        p.set_value(1);
        fmt::print("thread 1 set value...\n");
    });
    std::thread t2([&]() -> void {
        fmt::print("thread 2 start task...\n");
        std::this_thread::sleep_for(std::chrono::seconds(3));
        fmt::print("thread 2 get value: {}\n", p.get_future().get());
    });
    t1.join();
    t2.join();
    return 0;
}