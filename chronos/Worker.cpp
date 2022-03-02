#include <assert.h>
#include "Worker.hpp"

namespace chronos {
    Worker::~Worker() {
      assert(!running);
    }

    void Worker::sleep_until(app_time alarm_par) {
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
      runner->join();
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
      };
      running = false;
      working.unlock();
    }
}

