#include <vector>
#include <iostream>
#include <limits>
#include <fstream>
#include <algorithm>
#include <random>

#include "marketsim.hpp"
#include "marketsim/competition.hpp"
#include "marketsim/tests.hpp"
#include "msstrategies/tadpmarketmaker.hpp"
#include "msstrategies/naivemmstrategy.hpp"
#include "msstrategies/lessnaivemmstrategy.hpp"
#include "msstrategies/firstsecondstrategy.hpp"
#include "msstrategies/liquiditytakers.hpp"
#include "msstrategies/parametricmm.hpp"
#include "msstrategies/maslovorderplacer.hpp"
#include "msstrategies/initialstrategy.hpp"
#include "msdss/rawds.hpp"

using namespace marketsim;


int main()
{
    try
    {
        constexpr bool chronos = false;
        constexpr bool logging = false;
        constexpr tabstime runningtime = 100;
        twallet endowment(5000,100);
        tmarketdef def;

/*        def.loggingfilter.frequest = true;
        def.loggingfilter.fsleep = false;
        def.loggingfilter.fgetinfo = false;
        def.loggingfilter.fmarketdata = false;
        def.loggingfilter.ftick = false;
        def.loggingfilter.fabstime = false;*/
        def.loggingfilter.fsettle = true;
        def.loggingfilter.fds = true;

        def.loggedstrategies.push_back(0);
        def.directlogging = true;
        def.demandupdateperiod = 0.1;

    competitor<firstsecondstrategy<40,10>,chronos,true> fss;

    // this strategy simulates liquidity takers:
    // 360 times per hour on average it puts a market order with volume 5 on average
    competitor<liquiditytakers<360,5>,chronos,true> lts;


    // competing strategies

    // this strategy just wants to put orders into spread and when it
    // has more money than five times the price, it consumes. The volume of
    // the orders is always 10

        competitor<naivemmstrategy<10>,chronos> nmm("naivka");

        using testedstrategy = tgeneralpmm;
        competitor<testedstrategy,chronos> ts("tested");
        competitor<initialstrategy<90,110>,chronos,true> is("is");

//competitor<maslovstrategy,chronos> m;
        competitor<maslovorderplacer<100>,chronos> l("mop");
        twallet emptywallet = {0,0};
std::vector<double> o;
for(unsigned i = 0; i<20; i++)
{
        auto r = test<chronos,true,logging,rawds<3600,10>>({&is,&l,&nmm/*,&ts*/},2000,
                                                          {twallet::infinitewallet(),emptywallet,
                                                           endowment /*,endowment*/},
                                                           def);
        o.push_back(r->fhistory(2000).p());
        r->fhistory.output(std::clog,100);
}

for(unsigned i = 0; i<o.size(); i++)
    std::cout << o[i] << std::endl;

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



