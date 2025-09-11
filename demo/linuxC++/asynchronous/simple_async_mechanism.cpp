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

    void set_value(T* val) {
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
        return state_.get();
    }
};

template <typename T>
class promise {
    std::shared_ptr<assoc_state<T>> state_;

   public:
    explicit promise() : state_(std::make_shared<assoc_state<T>>()) {}

    future<T> get_future() {
        if (!state_) throw std::runtime_error("state_ = nullptr");
        return future<T>(state_);
    }
};

}  // namespace my_async