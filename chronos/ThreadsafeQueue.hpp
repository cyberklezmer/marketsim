#ifndef CHRONOS_THREADSAFEQUEUE_HPP
#define CHRONOS_THREADSAFEQUEUE_HPP


#include <queue>
#include <mutex>
#include <optional>

/** @file
 * definition of ThreadsafeQueue */

/**
 * Definition of Queue of pointers to the base type T.
 * the queue is thread safe e.g. multiple threads may access the queue
 * @tparam T base queue member (pointers to this type are stored in the queue)
 */
template<typename T>
class ThreadsafeQueue {
    std::queue<T> queue_;
    mutable std::mutex mutex_;

    // Moved out of public interface to prevent races between this
    // and pop().
    [[maybe_unused]] bool empty() const {
      return queue_.empty();
    }

 public:
    ThreadsafeQueue() = default;

    ThreadsafeQueue(const ThreadsafeQueue<T> &) = delete;

    ThreadsafeQueue &operator=(const ThreadsafeQueue<T> &) = delete;

    /**
     *  copy constructor
     * @param other ThreadsafeQueue to copy
     */
    ThreadsafeQueue(ThreadsafeQueue<T> &&other)  noexcept {
      std::lock_guard<std::mutex> lock(mutex_);
      queue_ = std::move(other.queue_);
    }

    virtual ~ThreadsafeQueue() = default;

    /**
     * Get the current size of the queue
     * @return size of the queue (number of elements)
     */
    unsigned long size() const {
      std::lock_guard<std::mutex> lock(mutex_);
      return queue_.size();
    }

    /**
     * Returns an element at the top of the queue or nullopt
     * @return element or nullopt in case of empty queue
     */
    std::optional<T> pop() {
      std::lock_guard<std::mutex> lock(mutex_);
      if (queue_.empty()) {
        return {};
      }
      T tmp = queue_.front();
      queue_.pop();
      return tmp;
    }

    /**
     * Pushes an element into the queue and (atomically) calls the `action` callback
     * @param item element to push into the queue
     * @param action std::function<void()>& to be called
     */
    void push_and_action(const T &item, const std::function<void()>& action) {
      std::lock_guard<std::mutex> lock(mutex_);
      queue_.push(item);
      action();
    }
};



#endif //CHRONOS_THREADSAFEQUEUE_HPP
