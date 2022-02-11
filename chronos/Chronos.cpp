#include <iostream>
#include "Chronos.hpp"
#include "Worker.hpp"

namespace chronos {
    void Chronos::register_worker(Worker *worker) {
      workers.push_back(worker);
    }

    void Chronos::run() {
      clock_time = 0;
      start_workers();
      loop();
    }

    void Chronos::loop() {
      app_time next_alarm = 1;
      while (still_running()) {
        tick_started();
        clock_time++;
        if (next_alarm <= clock_time)
          next_alarm = wake_workers();
        tick();
        wait_next_tick();
      }
    }

    bool Chronos::still_running() {
      return std::any_of(workers.begin(), workers.end(), [](auto worker) { return !!worker->running; });
    }

    /**
     * wake workers that have alarm current
     * @return minimal clock when some thread needs waking up (1 means next tick)
     */
    app_time Chronos::wake_workers() {
      app_time next_alarm = 0;
      for (auto worker: workers) {
        const guard lock(worker->alarm_handling);
        if (worker->alarm) {
          if (worker->alarm <= clock_time) {
            worker->alarm = 0;
            worker->waker.unlock();
            worker->working.lock();
            next_alarm = 1;
          }
          else if (!next_alarm || (worker->alarm < next_alarm))
            next_alarm = worker->alarm;
        }
        else
          next_alarm = 1;
      }
      return next_alarm;
    }

    void Chronos::start_workers() {
      for (auto worker: workers)
        worker->start();
    }

    /**
     * try to acquire all the working locks
     * if thread not running lock is acquired, otherwise we wait with maximum timeout of next tick
     * first loop returns index of first not-locked worker (worker that is still running)
     * if index < workers.size() we have timeouted (or it is already too late) so we can go on
     * if index == workers.size() we have all the locks e.g. all the workers are sleeping
     *   we do not need to wait and can go on wint next tick
     * anyway always release all the acquired locks
     */
    void Chronos::wait_next_tick() {
      int index;
      for (index = 0; index < workers.size(); index++) {
        if (!workers[index]->working.try_lock_until(next_tick))
          break;
      }
      for (index--; index >= 0; index--) {
        workers[index]->working.unlock();
      }
    }

    void Chronos::tick_started() {
      next_tick = std::chrono::steady_clock::now() + tick_duration;
    }

    unsigned long Chronos::format_time() {
      return std::chrono::steady_clock::now().time_since_epoch().count();
    }
}
