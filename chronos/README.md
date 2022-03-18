# Chronos

## Key features of `class Chronos`

| method                                                         | description                                                                                                                                                                                                                                                                                                                                                                             |
|----------------------------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `Chronos(app_duration duration, app_time max_time = 0)`        | Constructor, takes duration (system time) of single global tick as the first parameter <br/>and maximal number of ticks to run (zero means until all Workers finish)                                                                                                                                                                                                                    |
| `app_time get_time()`                                          | current global time (number of ticks since start)                                                                                                                                                                                                                                                                                                                                       |
| `bool running()`                                               | `Chronos` is still running (get_time < max_time)                                                                                                                                                                                                                                                                                                                                        |
| `virtual void tick()`                                          | user function called every clock tick (overridden in descendants)                                                                                                                                                                                                                                                                                                                       |
| `app_duration get_remaining_time()`                            | get system time remaining for this tick. if negative then we are overdue - increase duration set in `Chronos` constructor                                                                                                                                                                                                                                                               |
| `app_duration get_last_tick_duration()`                        | get last tick duration (in system time)                                                                                                                                                                                                                                                                                                                                                 |
| `void run(workers_list workers)`                               | starts the main loop  (`workers_list = std::vector<Worker *>`)                                                                                                                                                                                                                                                                                                                          |
| `void wait()`                                                  | Must be called before desctructing of `Worker`. It blocks until all worker threads are finished. When worker is destructed before its thread finishes, strange errors may appear (pure virtual method called, SIGTERM, ...)                                                                                                                                                             |
| `template<typename Functor>`<br/>`auto async(Functor functor)` | Register asynchronous call. The `Functor` is callable (probably a `lambda`) that returns any type and takes no parameters. `Functor` is marked for async execution by `Chronos`. Call to `async` blocks calling thread until the task is finished. Returns the same type as the `Functor`.<br/> throws `chronos::error_already_finished` if Chronos already finished (max_time passed). |


##Key features of `class Worker`


| method                                     | description                                                                                                                                                                                                           |
|--------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `Worker()`                                 | Constructor                                                                                                                                                                                                           |
| `virtual void main()`                      | Main working routine of worker <br/> may call `sleep_until` to give up CPU time <br/>runs in its own thread                                                                                                           |
| `bool ready()`                             | `Chronos` is still running                                                                                                                                                                                            |
| `bool still_running()`                     | This `Worker`'s thread is still running                                                                                                                                                                               |
| `void wait()`                              | Wait for `Worker`'s thread to finish                                                                                                                                                                                  |
| `void sleep_until(app_time alarm_par = 1)` | suspend the execution of this worker until `alarm_par` time <br/>without argument (default 1) sleep till next clock tick.<br/> If Worker is not used in any Chronos, calling of this function hangs and never returns |

## Usage

```c++
class MyChronos : public Chronos {
  public:
    void tick() override {
      //implement what to do every tick
      std::cout << "time passes so quickly " << get_time() << "\n";
    }
        
    vector<CLimitOrders> get_limit_orders(typ1 param1, typ2 param2){
      return async([this, param1, param2] () {
        // based on  param1, param2 + access to  `MyChronos`
        // generate the return value
        vector<CLimitOrders> ret_value;
        ret_value.push_back(item);
        // ...
        return ret_value;
      });
    }      
};

class MyWorker : public Worker {
  public:
    void main() override {
      //implement main working routine of the worker
      while i_should_be_running() {
        //maybe sometimes give up CPU
        //wait for next tick()
        sleep_until();
        
        // use asynchrounous functions from Chronos
        // this call may block our thread
        vector<CLimitOrders> orders = ((Stock&)parent).get_limit_orders(1,2);
       
        //wait 1000 ticks
        sleep_until(1000 + get_time());
      }
      //normally return when work done 
    }
};

MyChronos main_node(1000); //duration of tick
MyWorker 
  worker_1,
  worker_2,
  worker_3;

workers_list workers = {
    &worker_1,
    &worker_2,
    &worker_3
};
main_node.run(workers);

```