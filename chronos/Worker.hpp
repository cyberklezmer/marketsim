#ifndef MULTIPROCESS_WORKER_HPP
#define MULTIPROCESS_WORKER_HPP

#include <atomic>
#include <thread>
#include <mutex>

#include "Chronos.hpp"

/** @file */

namespace chronos {

    /**
     * Worker class
     *
     * implements behaviour of single worker
     *
     * descendants should override the @ref main method
     */
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

        void start();

     public:
        Worker() = default;

        virtual ~Worker();

        /**
         * This Worker has running working thread
         * @return bool
         */
        inline bool still_running() { return running; };

        /**
         * Waits for corresponding working thread to finish.
         * May block the caller.
         * Must be called from the main thread NOT from the context of the working thread itself
         */
        void wait();

     protected:
        /**
         * suspend the execution of this worker until `alarm_par` time
         *
         * without argument (default 1) or when the `alarm_par` already passed
         * then it sleeps till next clock tick
         * blocks the caller
         * @param alarm_par global tick count to sleep until
         */
        void sleep_until(app_time alarm_par = 1);

        /**
         * Main working routine of worker, may call:
         * - @ref sleep_until to give up CPU time
         * - Chronos::get_time to see global clock
         * - @ref ready to see if chronos is still running
         *
         * runs in its own thread \n
         * called by Chronos::run \n
         * should return when done computing
         */
        virtual void main() = 0;

        /**
         * Check if Chronos is still running
         * @return bool Chronos is not finished yet
         */
        inline bool ready() {
          return !finished;
        }
    };
}

#endif //MULTIPROCESS_WORKER_HPP
