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

      working.unlock();
      waker.lock();     //this will block
      working.lock();
    }

    void Worker::start() {
      running = true;
      std::thread runner(&Worker::entry_point, this);
      runner.detach();
    }

    void Worker::entry_point() {
      const guard_working lock(working);
      main();
      running = false;
    }
}

