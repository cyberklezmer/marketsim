#ifndef CHRONOS_THREADSAFEQUEUE_HPP
#define CHRONOS_THREADSAFEQUEUE_HPP


#include <queue>
#include <mutex>
#include <optional>

template<typename T>
class ThreadsafeQueue {
    std::queue<T> queue_;
    mutable std::mutex mutex_;

    // Moved out of public interface to prevent races between this
    // and pop().
    bool empty() const {
      return queue_.empty();
    }

 public:
    ThreadsafeQueue() = default;
    ThreadsafeQueue(const ThreadsafeQueue<T> &) = delete ;
    ThreadsafeQueue& operator=(const ThreadsafeQueue<T> &) = delete ;

    ThreadsafeQueue(ThreadsafeQueue<T>&& other) {
      std::lock_guard<std::mutex> lock(mutex_);
      queue_ = std::move(other.queue_);
    }

    virtual ~ThreadsafeQueue() { }

    unsigned long size() const {
      std::lock_guard<std::mutex> lock(mutex_);
      return queue_.size();
    }

    std::optional<T> pop() {
      std::lock_guard<std::mutex> lock(mutex_);
      if (queue_.empty()) {
        return {};
      }
      T tmp = queue_.front();
      queue_.pop();
      return tmp;
    }

    void push_and_action(const T &item, std::function<void()> action) {
      std::lock_guard<std::mutex> lock(mutex_);
      queue_.push(item);
      action();
    }
};



#endif //CHRONOS_THREADSAFEQUEUE_HPP
