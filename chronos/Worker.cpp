#include <assert.h>
#include "Worker.hpp"

namespace chronos {
    Worker::~Worker() {
      if (runner)
        runner->join();
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
      runner = new std::thread(&Worker::entry_point, this);
    }

    void Worker::entry_point() {
      try {
        main();
      }
      catch (...) {
      };
      running = false;
      working.unlock();
    }
}

