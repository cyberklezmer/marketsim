#include <assert.h>
#include "Worker.hpp"

namespace chronos {
    Worker::Worker(Chronos &main) : parent(main), running(false) {
      parent.register_worker(this);
    }

    Worker::~Worker() {
      assert(!running);
    }

    void Worker::sleep_until(app_time alarm_par) {
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
      running = true;
      working.lock();
      std::thread runner(&Worker::entry_point, this);
      runner.detach();
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

