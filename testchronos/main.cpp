#include <boost/math/distributions.hpp>
#include <vector>
#include <iostream>
#include <limits>
#include <fstream>
#include <algorithm>
#include <random>

#include "strategies.hpp"


using namespace marketsim;


int main()
{
    try
    {
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



