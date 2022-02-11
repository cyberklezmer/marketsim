##Threading

```std::thread```

master ma
counting semaphore
//pocet bezicich threadu
https://en.cppreference.com/w/cpp/thread/counting_semaphore


Working threads

```c++
#include <boost/thread/locks.hpp>
#include <boost/iterator/indirect_iterator.hpp>

#include <vector>
#include <mutex>

int main()
{
using mutex_list = std::vector<std::mutex*>;
mutex_list workings;

    boost::indirect_iterator<mutex_list::iterator> first(workings.begin()), 
                                                   last(workings.end());
    boost::lock(first, last);
}
```