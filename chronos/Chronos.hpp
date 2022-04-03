#ifndef MULTIPROCESS_CHRONOS_HPP
#define MULTIPROCESS_CHRONOS_HPP

#include <vector>
#include <mutex>
#include <future>
#include "ThreadsafeQueue.hpp"

/** @file */

/**
 * namespace chronos
 */
namespace chronos {
    class Worker;

    /**
     * Base class for Chronos exceptions
     */
    class error : public std::runtime_error {
     public:
        /**
         * constructor
         * @param what message of exception
         */
        explicit error(const std::string &what = "") : std::runtime_error(what) {}
    };

    /**
     *  the caller thread of async function is unknown
     */
    class error_unknown_thread : public error {
     public:
        error_unknown_thread() : error("Unknown thread") {}
    };

    /**
     *  chronos has already finished
     *  from Workers you should check either
     *  Chronos::running()        [should be true]
     *  Chronos::get_max_time()   [should be > Chronos::get_time()]
     */
    class error_already_finished : public error {
     public:
        error_already_finished() : error("Already finished") {}
    };

    /** application time (in number of ticks since startup) */
    using app_time = unsigned long long;

    /**  duration system dependant type for defining time interval */
    using app_duration = std::chrono::steady_clock::duration;

    /** system dependant type for defining point in the time */
    using app_time_point = std::chrono::steady_clock::time_point;

    /** lock guard of mutex (used internally by Chronos and Worker */
    using guard = std::lock_guard<std::mutex>;

    /** list of workers_ */
    using workers_list = std::vector<Worker *>;

    /**
     * Main Chronos class. Orchestrates all its workers_ and handles inter process communication.
     *
     * Should be constructed in the main thread of the application together with all the workers_ that are passed into
     * the @ref run method.
     *
     * Destruction may be done as soon as @ref run returns.
     */
    class Chronos {
     private:
        std::atomic<app_time> clock_time;
        std::atomic<bool> finished = false;
        app_time max_time_;
        workers_list workers_;
        app_duration last_tick_duration;
        app_time_point tick_start;
        app_duration tick_duration;
        ThreadsafeQueue<std::function<void()>> async_tasks;

        void start_workers();

        void wait_next_tick();

        app_time wake_workers(bool wake_all = false);

        bool still_running();

        void loop();

        void signal_finish();

        void signal_start();

        void process_async();

        void tick_started();

        [[maybe_unused]] static unsigned long format_time();

        void worker_unlock(int index);

        void worker_lock(int index);

        int get_thread_index();

     public:
        /**
         * Constructs a Chronos
         *
         * @param duration  - duration of one tick
         * @param max_time  - maximum number of ticks to run
         */
        explicit Chronos(app_duration duration, app_time max_time = 0);

        /**
         * Returns current global time (number of ticks since start)
         *
         * may be called by workers_
         * @return app_time
         */
        inline app_time get_time() {
          return clock_time;
        };

        /**
         * Returns the max time (number of tick) to run this chronos. Parameter `max_time` set in constructor Chronos::Chronos
         *
         * may be called by workers_
         * @return max_time
         */
        [[maybe_unused]] inline app_time get_max_time() const {
          return max_time_;
        };

        /**
         * returns true if chronos is still running
         *
         * may be called by workers_
         * @return bool
         */
        inline bool running() {
          return !finished;
        }

        /**
         * get time remaining in this tick. if negative then we are overdue - increase duration set in Chronos
         * constructor
         * @return app_duration
         */
        app_duration get_remaining_time() {
          return tick_start + tick_duration - std::chrono::steady_clock::now();
        };

        /**
         * get last tick duration
         * @return app_duration
         */
        [[maybe_unused]] inline app_duration get_last_tick_duration() {
          return last_tick_duration;
        };

        /**
         * starts the main loop
         */
        void run(workers_list workers);

        /**
         * wait for all the workers_ to finish
         * @param workers
         */
        [[maybe_unused]] static void wait(const workers_list &workers);

        /**
         * user function called in every clock tick
         *
         * overridden in descendants
         */
        virtual void tick() = 0;

        /**
         * passed Functor (probably lambda) is marked for async execution by Chronos
         * async blocks calling thread until the task is finished
         * returns same type as the Functor
         *
         * @tparam Functor
         * @param functor function returning ret type
         * @return ret
         */
        template<typename Functor>
        auto async(Functor functor) {
          using ret = decltype(functor());
          using task_t = std::packaged_task<ret()>;

          if (finished)
            throw error_already_finished();

          task_t task(functor);
          std::future<ret> future = task.get_future();
          auto t = std::make_shared<task_t>(std::move(task));
          int index = get_thread_index();

          async_tasks.push_and_action([t, this, index]() {
                                          worker_lock(index);
                                          (*t)();
                                      }, [this, index]() {
                                          worker_unlock(index);
                                      }
          );

          ret retval = future.get();
          return retval;
        }
    };

}
#endif //MULTIPROCESS_CHRONOS_HPP
