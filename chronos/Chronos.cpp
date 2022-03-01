#include <algorithm>
#include "Chronos.hpp"
#include "Worker.hpp"

namespace chronos {
    Chronos::Chronos(app_duration duration, app_time max_time) :
        tick_duration(duration),
        clock_time(0),
        max_time(max_time),
        last_tick_duration(0),
        tick_start(std::chrono::steady_clock::now()) {}

    void Chronos::run(workers_list workers) {
      clock_time = 0;
      this->workers = workers;
      start_workers();
      loop();
      signal_finish();
      process_async();
      wake_workers(true);
    }

    void Chronos::signal_finish() {
      finished = true;
      for (auto worker: workers)
        worker->finished = true;
    }

    void Chronos::loop() {
      app_time next_alarm = 1;
      while (still_running()) {
        tick_started();
        clock_time++;
        if (next_alarm <= clock_time)
          next_alarm = wake_workers();
        process_async();
        tick();
        wait_next_tick();
      }
    }

    void Chronos::process_async() {
      while (auto item = async_tasks.pop()) {
        item.value()();
      }
    }

    bool Chronos::still_running() {
      return (!max_time || (max_time <= clock_time)) &&
             std::any_of(workers.begin(), workers.end(), [](auto worker) { return !!worker->running; });
    }

    /**
     * wake workers that have alarm current
     * @return minimal clock when some thread needs waking up (1 means next tick)
     */
    app_time Chronos::wake_workers(bool wake_all) {
      app_time next_alarm = 0;
      for (auto worker: workers) {
        const guard lock(worker->alarm_handling);
        if (worker->alarm) {
          if (wake_all || (worker->alarm <= clock_time)) {
            worker->alarm = 0;
            worker->working.lock();
            worker->waker.unlock();
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
      app_time_point next_tick = tick_start + tick_duration;
      for (index = 0; index < workers.size(); index++) {
        if (!workers[index]->working.try_lock_until(next_tick))
          break;
      }
      for (index--; index >= 0; index--) {
        worker_unlock(index);
      }
    }

    void Chronos::worker_unlock(int index) {
      workers[index]->working.unlock();
    }

    void Chronos::worker_lock(int index) {
      workers[index]->working.lock();
    }

    int Chronos::get_thread_index() {
      std::thread::id cur_id = std::this_thread::get_id();
      int index;
      for (index = 0; index < workers.size(); index++) {
        if (workers[index]->runner->get_id() == cur_id)
          return index;
      }
      throw error_unknown_thread();
    }

    void Chronos::tick_started() {
      app_time_point now = std::chrono::steady_clock::now();
      last_tick_duration = now - tick_start;
      tick_start = now;
    }

    unsigned long Chronos::format_time() {
      return std::chrono::steady_clock::now().time_since_epoch().count();
    }

}
