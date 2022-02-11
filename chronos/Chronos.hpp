#ifndef MULTIPROCESS_CHRONOS_HPP
#define MULTIPROCESS_CHRONOS_HPP

#include <boost/thread/locks.hpp>
#include <boost/iterator/indirect_iterator.hpp>

#include <vector>
#include <mutex>

namespace chronos {
    class Worker;

    using app_time = unsigned long long;
    using app_duration = std::chrono::steady_clock::duration;

    class Chronos {
     private:
        std::atomic<app_time> clock_time;
        std::vector<Worker *> workers;
        std::chrono::steady_clock::time_point next_tick;
        app_duration tick_duration;

        void start_workers();

        void wait_next_tick();

        app_time wake_workers();

        bool still_running();

        void loop();

        void tick_started();

        unsigned long format_time();

     public:
        Chronos(app_duration duration) : tick_duration(duration), clock_time(0) {};

        inline app_time get_time() {
          return clock_time;
        };

        //worker registers itself
        void register_worker(Worker *worker);

        //start main loop
        void run();

        // user function called every clock tick
        virtual void tick() = 0;
    };

}
#endif //MULTIPROCESS_CHRONOS_HPP
