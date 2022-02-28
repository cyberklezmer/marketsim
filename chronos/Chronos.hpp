#ifndef MULTIPROCESS_CHRONOS_HPP
#define MULTIPROCESS_CHRONOS_HPP

#include <vector>
#include <mutex>
#include <future>
#include "ThreadsafeQueue.hpp"

namespace chronos {
    class Worker;

    using app_time = unsigned long long;
    using app_duration = std::chrono::steady_clock::duration;
    using app_time_point = std::chrono::steady_clock::time_point;
    using guard = std::lock_guard<std::mutex>;
    using workers_list = std::vector<Worker *>;

    class Chronos {
     private:
        std::atomic<app_time> clock_time;
        std::atomic<bool> finished = false;
        app_time max_time;
        workers_list workers;
        app_duration last_tick_duration;
        app_time_point tick_start;
        app_duration tick_duration;
        ThreadsafeQueue<std::function<void()>> async_tasks;

        void start_workers();

        void wait_next_tick();

        app_time wake_workers();

        bool still_running();

        void loop();

        void process_async();

        void tick_started();

        unsigned long format_time();

        void worker_unlock(int index);

        void worker_lock(int index);

        int get_thread_index();

     public:
        /**
         * Constructs a Chronos
         * @param duration of one tick
         */
        Chronos(app_duration duration, app_time max_time = 0);

        /**
         * current global time (number of ticks since start)
         * @return app_time (unsigned long long)
         */
        inline app_time get_time() {
          return clock_time;
        };

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
        inline app_duration get_last_tick_duration() {
          return last_tick_duration;
        };

        /**
         * starts main loop
         */
        void run(workers_list workers);

        /**
         * user function called every clock tick
         * overridden in descendants
         */
        virtual void tick() = 0;

        /**
         * passed Functor (probably lambda) is marked for async execution by Chronos
         * async blocks calling thread until the task is finished
         * returns same type as the Functor
         *
         * @tparam Functor
         * @param function returning ret type
         * @return ret
         */
        template<typename Functor>
        auto async(Functor functor) {
          using ret = decltype(functor());
          using task_t = std::packaged_task<ret()>;

          if (finished)
            throw std::runtime_error("Already finished");

          task_t task(functor);
          std::future<ret> future = task.get_future();
          auto t = std::make_shared<task_t>(std::move(task));
          int index = get_thread_index();

          async_tasks.push([t]() { (*t)(); });

          worker_unlock(index);
          ret retval = future.get();
          worker_lock(index);

          return retval;
        }
    };

}
#endif //MULTIPROCESS_CHRONOS_HPP
