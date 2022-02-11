# Chronos

## Key features of `class Chronos`

| method                           | description                                                       |
|----------------------------------|-------------------------------------------------------------------|
| `Chronos(app_duration duration)` | Constructor, takes duration of single global tick as a parameter  |
| `app_time get_time()`            | current global time (number of ticks since start)                 |
| `virtual void tick()`            | user function called every clock tick (overridden in descendants) |
| `void run()`                     | starts main loop                                                  |


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
};

class MyWorker : public Worker {
    public:
        void main() override {
            //implement main working routine of the worker
            while i_should_be_running()
            {
              //maybe sometimes give up CPU
              //wait for next tick()
              sleep_until();
              
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