#include <vector>
#include <iostream>
#include <limits>
#include <fstream>
#include <algorithm>
#include <random>

#include "mscompetitions/separatecompetition.hpp"
#include "msstrategies/adpmarketmaker.hpp"
#include "msstrategies/naivemmstrategy.hpp"
// #include "msstrategies/lessnaivemmstrategy.hpp"
#include "msstrategies/heuristicstrategy.hpp"
#include "msstrategies/trendspeculator.hpp"
#include "msstrategies/technicalanalyst.hpp"

//#include "torch/torch.h"
#include "config.hpp"

using namespace marketsim;


int main()
{
    try
    {
        constexpr bool logging = true;

        competitor<naivemmstrategy<10>> cm("naivka");
        competitor<heuristicstrategy<false>> h("heuristic");
        competitor<adpmarketmaker> am("adpmm");
        competitor<neuralstrategy> ns("neuronka");

        competitor<spec_neuralstrategy> nspec("neuronspeculator");
        competitor<trendspeculator> tspec("trendspeculator");
        competitor<ta_macd> techanal("techanal");

        separatezicomp<logging>({&cm,&h,&am,&ns,&nspec,&tspec,&techanal});
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



