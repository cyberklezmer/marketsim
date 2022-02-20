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
    using guard = std::lock_guard<std::mutex>;

    class Chronos {
     private:
        std::atomic<app_time> clock_time;
        std::vector<Worker *> workers;
        std::chrono::steady_clock::time_point next_tick;
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

     public:
        /**
         * Constructs a Chronos
         * @param duration of one tick
         */
        Chronos(app_duration duration) : tick_duration(duration), clock_time(0) {};

        /**
         * current global time (number of ticks since start)
         * @return app_time (unsigned long long)
         */
        inline app_time get_time() {
          return clock_time;
        };

        //worker registers itself
        void register_worker(Worker *worker);

        /**
         * starts main loop
         */
        void run();

        /**
         * user function called every clock tick
         * overridden in descendants
         */
        virtual void tick() = 0;

        /**
         * passed Functor (probably lambda) is marked for async execution by Chronos
         * this call then blocks calling thread until the task is finished
         * returns same type as the Functor
         *
         * @tparam Functor
         * @param function returning ret type
         * @return ret
         */
        template<typename Functor>
        auto async(Functor functor){
          using ret = decltype(functor());
          using task_t = std::packaged_task<ret()>;

          task_t task(functor);
          std::future<ret> future = task.get_future();
          auto t = std::make_shared<task_t>(std::move(task));

          async_tasks.push([t]() { (*t)(); });

          //TODO unflag working from calling Worker
          ret retval = future.get();
          //TODO reflag working from calling Worker
          return retval;
        }
    };

}
#endif //MULTIPROCESS_CHRONOS_HPP
