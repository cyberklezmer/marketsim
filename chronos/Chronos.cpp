#include <algorithm>
#include <utility>
#include "Chronos.hpp"
#include "Worker.hpp"

/** @file */

namespace chronos {

    using guard = std::lock_guard<std::mutex>;

    Chronos::Chronos(app_duration duration, app_time max_time) :
        tick_duration(duration),
        clock_time(0),
        max_time_(max_time),
        last_tick_duration(0),
        tick_start(std::chrono::steady_clock::now()) {}

    void Chronos::run(workers_list workers) {
      workers_ = std::move(workers);
      start_workers();
      signal_start();
      loop();
      signal_finish();
      process_async();
      wake_workers(true);
      workers_.clear();
    }

    [[maybe_unused]] void Chronos::wait(const workers_list& workers) {
      for (auto worker: workers)
        worker->wait();
    }

    void Chronos::signal_start() {
      for (auto worker: workers_)
        worker->alarm_handling.unlock();
    }

    void Chronos::signal_finish() {
      finished = true;
      for (auto worker: workers_)
        worker->finished = true;
    }

    void Chronos::loop() {
      app_time next_alarm = 1;
      while (still_running()) {
        clock_time++;
        tick_started();
        if (next_alarm <= clock_time)
          next_alarm = wake_workers();
        process_async();
        tick();
        wait_next_tick();
      }
    }

    void Chronos::process_async() {
      unsigned long items = async_tasks.size();
      for (unsigned long i = 0; i < items; i++)
        async_tasks.pop().value()();
    }

    /**
     * Tests if Chronos should be still running
     * current clock_time must be lower than max_time (if defined)
     * AND any of workers_ is still running
     * @return bool
     */
    bool Chronos::still_running() {
      return (!max_time_ || (clock_time <= max_time_)) &&
             std::any_of(workers_.begin(), workers_.end(), [](auto worker) { return (bool) worker->running; });
    }

    /**
     * wake workers_ that have alarm current
     * @return app_time minimal clock when some thread needs waking up (1 means next tick)
     */
    app_time Chronos::wake_workers(bool wake_all) {
      app_time next_alarm = 0;
      for (auto worker: workers_) {
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

    /**
     * Start all the workers_ (spawns its threads)
     */
    void Chronos::start_workers() {
      for (auto worker: workers_)
        worker->start();
    }

    /**
     * Waits for next tick (if some worker is running) or perform "fast-forward" to next tick if all the workers_ are
     * sleeping.
     *
     * First try to acquire all the working locks.
     * If a thread is not running then its lock is acquired, otherwise we wait with maximum timeout of next tick start.
     * first loop returns index of first not-locked worker (worker that is still running)
     * if index < workers_.size() we have time-outed (or it is already too late) so we can go on
     * if index == workers_.size() we have all the locks e.g. all the workers_ are sleeping
     *   we do not need to wait and can go on in the next tick
     * anyway always release all the acquired locks at the end
     */
    void Chronos::wait_next_tick() {
      int index;
      app_time_point next_tick = tick_start + tick_duration;
      for (index = 0; index < workers_.size(); index++) {
        if (!workers_[index]->working.try_lock_until(next_tick))
          break;
      }
      for (index--; index >= 0; index--) {
        worker_unlock(index);
      }
    }

    void Chronos::worker_unlock(int index) {
      workers_[index]->working.unlock();
    }

    void Chronos::worker_lock(int index) {
      workers_[index]->working.lock();
    }

    int Chronos::get_thread_index() {
      std::thread::id cur_id = std::this_thread::get_id();
      int index;
      for (index = 0; index < workers_.size(); index++) {
        if (workers_[index]->runner->get_id() == cur_id)
          return index;
      }
      throw error_unknown_thread();
    }

    void Chronos::tick_started() {
      app_time_point now = std::chrono::steady_clock::now();
      last_tick_duration = now - tick_start;
      tick_start = now;
    }

    /** helper function used for debugging */
    [[maybe_unused]] unsigned long Chronos::format_time() {
      return std::chrono::steady_clock::now().time_since_epoch().count();
    }

}
