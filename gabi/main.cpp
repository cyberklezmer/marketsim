#include <vector>
#include <iostream>
#include <limits>
#include <fstream>
#include <algorithm>
#include <random>

#include "marketsim.hpp"
#include "marketsim/competition.hpp"
#include "mscompetitions/dsmaslovcompetition.hpp"
#include "mscompetitions/originalcompetition.hpp"
#include "marketsim/tests.hpp"
#include "msstrategies/naivemmstrategy.hpp"
#include "msstrategies/liquiditytakers.hpp"
#include "msstrategies/firstsecondstrategy.hpp"
#include "msstrategies/neuralnetstrategy.hpp"
#include "msstrategies/greedystrategy.hpp"
#include "msstrategies/initialstrategy.hpp"
#include "msstrategies/maslovstrategy.hpp"

#include "nets/actorcritic.hpp"
#include "nets/trainer.hpp"
#include "nets/rewards.hpp"

#include <torch/torch.h>

using namespace marketsim;

int main()
{
    torch::manual_seed(42);

    torch::Tensor tensor = torch::rand({2, 3});
    std::cout << tensor << std::endl;

    try
    {
        // change accordingly (but true is still beta),
        constexpr bool chronos = false;

        // change accordingly
        constexpr bool logging = true;

        // change accordingly (one unit rougly corresponds to one second)
        constexpr tabstime runningtime = 1000;
        //constexpr tabstime runningtime = 8 * 3600;

        // change accordingly
        twallet endowment(5000,100);

        tmarketdef def;

        // you can modify def, perhaps adjust logging etc

        // change accordingly but note that with chronos==true the streategies must be
        // descentants of marketsim::teventdrivenstrategy
        //using testedstrategy = maslovstrategy;

        bool with_mm = true;

        constexpr int n_steps = 2;
        constexpr int cons_mult = 10000;
        constexpr int cons_lim = 1000;
        constexpr int keep_stocks = 10;
        constexpr int spread_lim = 10;

        constexpr int volume = 10;
        constexpr bool modify_cons = true;

        constexpr int max_consumption = 1000;
        constexpr int cons_parts = 4;
        constexpr bool entropy_reg = true;
        
        using network = ACDiscrete<4, 256, 10, max_consumption, cons_parts>;
        //using network = ACContinuous<4, 256, 1, cons_mult>;

        constexpr int money_div = 1000;
        constexpr int stock_div = 10; 

        using returns_func = WeightedDiffReturn<n_steps, money_div, stock_div>;
        //using returns_func = DiffReturn<n_steps>;
        
        using trainer = NStepTrainer<network, n_steps, returns_func, entropy_reg>;

        using testedstrategy = neuralnetstrategy<trainer, cons_lim, keep_stocks, spread_lim, modify_cons, volume>;
          // should be the AI strategy
        //using testedstrategy = greedystrategy<>;

        using secondtestedstrategy = naivemmstrategy<10>;
        std::string sname = "naivka";

        enum ewhattodo { esinglerunsinglestrategy,
                         erunall,
                         ecompetition,
                         esinglerunstrategyandmaslovwithlogging,
                         edetailedcompetiton,
                         emaslovcompetition,
                         eoriginalcompetition };


        // change accordingly
        ewhattodo whattodo = emaslovcompetition;

        // built in strategies

        // this strategy makes 40 random orders during the first second
        std::string fsname = "fsko_";
        competitor<firstsecondstrategy<40,10>,chronos,true> fss(fsname);

        // this strategy simulates liquidity takers:
        // 360 times per hour on average it puts a market order with volume 5 on average
        std::string ltname = "ltcko_";
        competitor<liquiditytakers<360,5>,chronos,true> lts(ltname);


        // competing strategies


        // this strategy just wants to put orders into spread and when it
        // has more money than five times the price, it consumes. The volume of
        // the orders is always 10

        std::string nname = "naivka_";
        competitor<naivemmstrategy<10>,chronos> nmm(nname);

        // our ingenious strategy
        std::string name = "neuronka_";
        competitor<testedstrategy,chronos> ts(name);

        switch(whattodo)
        {
        case esinglerunsinglestrategy:
            {
                competitor<testedstrategy,chronos> s;
                test<chronos,true,logging>({&s},runningtime,endowment,def);
            }
            break;
        case erunall:
            if (with_mm) {
                test<chronos,true,logging>({&fss,&lts,&nmm,&ts}, runningtime, endowment, def);
            }
            else {
                test<chronos,true,logging>({&fss,&lts,&ts}, runningtime, endowment, def);
            }
            break;
        case ecompetition:
        {
            tcompetitiondef cdef;
            cdef.timeofrun = runningtime;
            cdef.marketdef = def;
            if (with_mm) {
                    competition<chronos,true,logging>(
                        {&fss,&lts,&nmm,&ts},
                        {twallet::infinitewallet(),twallet::infinitewallet(),endowment,endowment},
                        cdef, std::clog
                    );
            }
            else {
                    competition<chronos,true,logging>(
                        {&fss,&lts,&ts}, 
                        {twallet::infinitewallet(),twallet::infinitewallet(),endowment},
                        cdef, std::clog
                    );
            }
        }
            break;
        case esinglerunstrategyandmaslovwithlogging:
            {
                // runs the tested strategy
                competitor<testedstrategy,chronos> s(name);
                competitor<maslovstrategy,chronos> m("msdlob");
                // we will log the settlements
                def.loggingfilter.fsettle = true;
                // we wish to log also log entries by m, so we add its index
                // (see m's event handler to see how strategy can issue log entries)
                def.loggedstrategies.push_back(1);
                // the log is sent to std::cout
                auto data = test<chronos,true,true>({&s,&m},runningtime,endowment,def);
                const tmarkethistory& h=data->fhistory;
                tabstime t = 0;
                unsigned n = 30;
                tabstime dt = runningtime / n;

                std::cout << "t,b,a,p,definedp" << std::endl;
                for(unsigned i=0; i<=n; i++, t+=dt)
                {
                    auto s = h(t);
                    std::cout << t << "," << s.b << "," << s.a << "," << s.p() << ",";
                    if(i)
                        std::cout << h.definedpin(t-dt,t);
                    std::cout << std::endl;
                }
            }
            break;
        case edetailedcompetiton:
            {
            // competition of three naive market makers with different volume

            // this strategy puts two quotes in the beginning
            competitor<initialstrategy<90,100>,chronos,true> fss("initial");

            // this strategy simulates liquidity takers:
            // 360 times per hour on average it puts a market order with volume 5 on average
            competitor<liquiditytakers<3600,10>,chronos,true> lts("lts");


            // competing strategies

            // this strategy just wants to put orders into spread and when it
            // has more money than five times the price, it consumes. The volume of
            // the orders is always 10

            competitor<naivemmstrategy<1>,chronos> nmm1("naivka1_");
            competitor<naivemmstrategy<10>,chronos> nmm10("naivka10_");
            competitor<naivemmstrategy<50>,chronos> nmm50("naivka50_");


            tcompetitiondef cdef;
            cdef.timeofrun = runningtime;
            cdef.marketdef = def;
            cdef.samplesize = 20;
            competition<chronos,true,logging>(
                             {&fss,&lts,&nmm1,&nmm10,&nmm50,&ts},
                             {twallet::infinitewallet(),twallet::infinitewallet(),
                              endowment,endowment,endowment,endowment},
                              cdef, std::clog);

            }
            break;
        case emaslovcompetition:
        case eoriginalcompetition:
            {
                competitor<testedstrategy,chronos> fs(name);
                competitor<secondtestedstrategy,chronos> ss(sname);

                // we will log the settlements
                def.loggingfilter.fsettle = true;
                // we wish to log also log entries by m, so we add its index
                // (see m's event handler to see how strategy can issue log entries)
                def.loggedstrategies.push_back(0);

                tcompetitiondef cdef;
                cdef.timeofrun = runningtime;
                cdef.marketdef = def;

                if(whattodo==emaslovcompetition)
                    dsmaslovcompetition<chronos,logging>({&fs,&ss}, endowment, cdef, std::clog);
                else
                    originalcompetition<chronos,logging>({&fs,&ss}, endowment, cdef, std::clog);
            }
            break;
        default:
            throw "unknown option";
        }
        
        std::clog << "Done!" << std::endl;
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
