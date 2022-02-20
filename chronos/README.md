# Chronos

## Key features of `class Chronos`

| method                                                         | description                                                                                                                                                                                                      |
|----------------------------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `Chronos(app_duration duration)`                               | Constructor, takes duration of single global tick as a parameter                                                                                                                                                 |
| `app_time get_time()`                                          | current global time (number of ticks since start)                                                                                                                                                                |
| `virtual void tick()`                                          | user function called every clock tick (overridden in descendants)                                                                                                                                                |
| `void run()`                                                   | starts main loop                                                                                                                                                                                                 |
| `template<typename Functor>`<br/>`auto async(Functor functor)` | Register asynchronous call. The `Functor` passed (probably a `lambda`) is marked for async execution by `Chronos`. `async` blocks calling thread until the task is finished. Returns same type as the `Functor`. |


##Key features of `class Worker`


| method                                     | description                                                                                                                                         |
|--------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------|
| `Worker(Chronos &main)`                    | Constructor, takes reference to Chronos class (automatically registers itself)                                                                      |
| `virtual void main()`                      | Main working routine of worker <br/> may call `sleep_until` to give up CPU time <br/>may call `get_time` to see global clock<br/>runs in own thread |
| `void sleep_until(app_time alarm_par = 1)` | suspend the execution of this worker until `alarm_par` time <br/>without argument (default 1) sleep till next clock tick                            |
| `app_time get_time()`                      | reurns current tick time - passes to Chronos::get_time()                                                                                            |          

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
  worker_1(main_node),
  worker_2(main_node),
  worker_3(main_node);

main_node.run();

```