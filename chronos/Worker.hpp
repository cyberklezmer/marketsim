#ifndef MULTIPROCESS_WORKER_HPP
#define MULTIPROCESS_WORKER_HPP

#include <atomic>
#include <thread>
#include <mutex>

#include "Chronos.hpp"

namespace chronos {
    class Worker {
        friend Chronos;
     private:

        //signal this working thread is running
        std::timed_mutex working;

        //alarm clock of worker
        std::mutex waker;

        //guards alarm
        std::mutex alarm_handling;
        app_time alarm = 0;

        //this Worker has running working thread
        std::atomic<bool> running;

        void entry_point();

     protected:
        Chronos &parent;

     public:
        Worker(Chronos &main);

        virtual ~Worker();

        //called by Chronos
        void start();

     protected:
        /**
         * returns the global wall time
         * @return app_time
         */
        inline app_time get_time() {
          return parent.get_time();
        };

        /**
         * suspend the execution of this worker until <alarm_par> time
         * without argument sleep till next clock tick
         * may block the caller
         */
        void sleep_until(app_time alarm_par = 1);

        /**
         * Main working routine of worker
         * may call sleep_until to give up CPU time
         * may call get_time to see global clock
         * runs in own thread
         * returns when done computing
         */
        virtual void main() = 0;
    };
}

#endif //MULTIPROCESS_WORKER_HPP
