#include <cassert>
#include "Worker.hpp"

/** @file */

namespace chronos {
    Worker::~Worker() {
      wait();
      assert(!running);
    }

    void Worker::sleep_until(app_time alarm_par) {
      if (!alarm_par)
        alarm_par = 1;
      if (finished)
        return;
      {
        const guard lock(alarm_handling);
        waker.lock();
        alarm = alarm_par;
      }

      working.unlock(); //we stop working (locked again by Chronos when he wakes us back)
      waker.lock();     //this will block
      waker.unlock();   //ready for next round
    }

    /**
     * Start thread for this worker
     * called by Chronos in Chronos::start_workers
     */
    void Worker::start() {
      assert(!running); //same worker used multiple times?
      assert(!runner);
      running = true;
      finished = false;
      working.lock();
      alarm_handling.lock();
      runner = new std::thread(&Worker::entry_point, this);
    }

    void Worker::wait() {
      if (runner) {
        runner->join();
        delete runner;
        runner = nullptr;
      }
    }

    void Worker::entry_point() {
      {
        //wait for start
        const guard lock(alarm_handling);
      }
      try {
        main();
      }
      catch (...) {
        // all exception from main are silently ignored
      }
      running = false;
      working.unlock();
    }
}

