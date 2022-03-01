#include <boost/math/distributions.hpp>
#include <vector>
#include <iostream>
#include <limits>
#include <fstream>
#include <algorithm>
#include <random>

//#include "strategies.hpp"
#include "marketsim.hpp"

using namespace marketsim;
using namespace std;

class foostrategy: public tstrategy
{
public:
       foostrategy() : tstrategy("foo") {}

       virtual void trade(twallet) override
       {
          for(;;)
          {
              sleepfor(10);
          }
      }
};


int main()
{
    try
    {
        tmarket m(10);

        foostrategy f;
        twallet e(10,10);

        std::cout << "start" << std::endl;
        m.run({&f},{e});
        std::cout << "stop" << std::endl;

        return 0;
    }
    catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    catch (const char* m)
    {
           std::cerr << m << std::endl;
           return 1;
    }
    catch (const std::string& m)
    {
           std::cerr << m << std::endl;
           return 1;
    }
    catch(...)
    {
        std::cerr << "unknown exception" << std::endl;
        return 1;
    }

    return 0;
}



