#include <vector>
#include <iostream>
#include <limits>
#include <fstream>
#include <algorithm>
#include <random>

#include "marketsim.hpp"
#include "msstrategies/marketorderplacer.hpp"
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
#include "msstrategies/marketorderplacer.hpp"
#include "msdss/rawds.hpp"

using namespace marketsim;


int main()
{
    try
    {
        constexpr bool chronos = false;
        constexpr bool logging = true;
        constexpr tabstime runningtime = 100;
        twallet endowment(5000,100);
        tmarketdef def;

/*        def.loggingfilter.frequest = true;
        def.loggingfilter.fsleep = false;
        def.loggingfilter.fgetinfo = false;
        def.loggingfilter.fmarketdata = false;
        def.loggingfilter.ftick = false;
        def.loggingfilter.fabstime = false;
        def.loggingfilter.fsettle = true;
        def.loggingfilter.fds = true;*/
        def.loggingfilter.fprotocol = true;

        def.loggingfilter.fsettle = true;
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

        competitor<lessnaivemmstrategy<10,3600*5>,chronos> nmm("naivka");

        using testedstrategy = tgeneralpmm;
        competitor<testedstrategy,chronos> ts("tested");
        competitor<initialstrategy<90,110>,chronos,true> is("is");

//competitor<maslovstrategy,chronos> m;
        //competitor<cancellingmaslovorderplacer<100,20>,chronos> l("mop");
        competitor<cancellingmarketorderplacer<100,true,true>,chronos> l("mop");
        twallet emptywallet = {0,0};
std::vector<double> o;
for(unsigned i = 0; i<1; i++)
{
        auto r = test<chronos,true,logging,rawds<3600,3>>({&is,&l,&nmm/*,&ts*/},50,
                                                          {twallet::infinitewallet(),emptywallet,
                                                            endowment/* ,endowment*/},
                                                           def);
//for(unsigned j=0; j<r->fhistory.x().size(); j++)
//{
//    auto a = r->fhistory.x()[j];
//    std::cout  << a.t << " " << a.a << " " << a.b << std::endl;
//}
//        o.push_back(r->fhistory(20000).p());
//        r->protocol(std::clog,20000);
}

//for(unsigned i = 0; i<o.size(); i++)
//    std::cout << o[i] << std::endl;

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



