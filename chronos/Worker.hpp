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
        std::atomic<bool> running = false;
        std::thread *runner = nullptr;
        //Chronos is still alive
        std::atomic<bool> finished = false;

        void entry_point();

        //called by Chronos
        void start();

        void wait();

     public:
        Worker() {};

        virtual ~Worker();

     protected:
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

        /**
         * Chronos is still running
         */
        inline bool ready() {
          return !finished;
        }
    };
}

#endif //MULTIPROCESS_WORKER_HPP
