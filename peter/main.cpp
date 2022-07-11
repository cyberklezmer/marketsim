#include <vector>
#include <iostream>
#include <limits>
#include <fstream>
#include <algorithm>
#include <random>

#include "marketsim.hpp"
#include "marketsim/competition.hpp"
#include "marketsim/tests.hpp"
#include "msstrategies/naivemmstrategy.hpp"
#include "msstrategies/maslovstrategy.hpp"
#include "msstrategies/stupidtraders.hpp"
#include "mscompetitions/dsmaslovcompetition.hpp"
#include "mscompetitions/originalcompetition.hpp"
#include "msstrategies/technicalanalyst.hpp"
#include "msstrategies/buyer.hpp"
#include "msstrategies/adpmarketmaker.hpp"
#include "msstrategies/trendspeculator.hpp"

using namespace marketsim;

int main()
{
    try
    {
        // change accordingly (but true is still beta),
        constexpr bool chronos = false;

        // change accordingly
        constexpr bool logging = false;

        // change accordingly (one unit rougly corresponds to one second)
        constexpr tabstime runningtime = 100;

        // change accordingly
        twallet competitorsendowment(1000, 100);

        tmarketdef def;

        // you can modify def, perhaps adjust logging etc

        // change accordingly but note that with chronos==true the streategies must be
        // descentants of marketsim::teventdrivenstrategy
        using testedstrategy = naivemmstrategy<1>;
        std::string tsname = "naivka1";
        using secondtestedstrategy = naivemmstrategy<1>;
        std::string sectsname = "naivka2";

        enum ewhattodo {
            esinglerunsinglestrategy,
            esinglerunstrategyandmaslovwithlogging,
            edetailedcompetiton,
            emaslovcompetition,
            eoriginalcompetition,
            ebuyerscompetition
        };

        // change accordingly
        ewhattodo whattodo = edetailedcompetiton;

        switch (whattodo)
        {
        case esinglerunsinglestrategy:
        {
            // runs one simpulation with the only stragegy s (which most probabily
            // will not lead to any trading)
            competitor<testedstrategy, chronos> s;
            test<chronos, true, logging>({ &s }, runningtime, competitorsendowment, def);
        }
        break;
        case esinglerunstrategyandmaslovwithlogging:
        {
            // runs the tested strategy
            competitor<testedstrategy, chronos> s(tsname);
            competitor<maslovstrategy, chronos> m("msdlob");
            // we will log the settlements
            def.loggingfilter.fsettle = true;
            // we wish to log also log entries by m, so we add its index
            // (see m's event handler to see how strategy can issue log entries)
            def.loggedstrategies.push_back(1);
            // the log is sent to std::cout
            auto data = test<chronos, true, true>({ &s,&m }, runningtime, competitorsendowment, def);
            const tmarkethistory& h = data->fhistory;
            tabstime t = 0;
            unsigned n = 30;
            tabstime dt = runningtime / n;

            std::cout << "t,b,a,p,definedp" << std::endl;
            for (unsigned i = 0; i <= n; i++, t += dt)
            {
                auto s = h(t);
                std::cout << t << "," << s.b << "," << s.a << "," << s.p() << ",";
                if (i)
                    std::cout << h.definedpin(t - dt, t);
                std::cout << std::endl;
            }
        }
        break;
        case edetailedcompetiton:
        {
            // competition of three naive market makers with different volume

            // this strategy puts two quotes in the beginning
            competitor<initialstrategy<90, 100>, chronos, true> fss("initial");

            // this strategy simulates liquidity takers:
            // 360 times per hour on average it puts a market order with volume 5 on average
            competitor<liquiditytakers<3600, 10>, chronos, true> lts("lts");


            // competing strategies

            // this strategy just wants to put orders into spread and when it
            // has more money than five times the price, it consumes. The volume of
            // the orders is always 10

            competitor<naivemmstrategy<1>, chronos> nmm1("naivka1");
            competitor<naivemmstrategy<10>, chronos> nmm10("naivk10");
            competitor<naivemmstrategy<50>, chronos> nmm50("naivk50");
            competitor<adpmarketmaker, chronos> adpmm("adpmm");
            competitor<ta_macd, chronos> macd("macd");
            competitor<buyer, chronos> buyer("buyer");
            competitor<trendspeculator, chronos> trendspeculator("tspec");

            tcompetitiondef cdef;
            cdef.timeofrun = runningtime;
            cdef.marketdef = def;
            cdef.samplesize = 20;
            competition<chronos, true, logging>(
                { &fss,&lts,&nmm1,&nmm10,&nmm50, &adpmm, &macd,&trendspeculator},
                { twallet::infinitewallet(),twallet::infinitewallet(),
                 competitorsendowment,competitorsendowment,competitorsendowment,competitorsendowment,
                competitorsendowment,competitorsendowment },
                cdef, std::clog);

        }
        break;
        case emaslovcompetition:
        case eoriginalcompetition:
        {
            competitor<testedstrategy, chronos> fs(tsname);
            competitor<secondtestedstrategy, chronos> ss(sectsname);
            tcompetitiondef cdef;
            cdef.timeofrun = runningtime;
            cdef.marketdef = def;
            if (whattodo == emaslovcompetition)
                dsmaslovcompetition<chronos, logging>({ &fs,&ss }, competitorsendowment, cdef, std::clog);
            else
                originalcompetition<chronos, logging>({ &fs,&ss }, competitorsendowment, cdef, std::clog);
        }
        break;
        case ebuyerscompetition:
        {
            /// work under progress
            competitor<stupidtrader<1, 2, true>, chronos> st("st");
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
    catch (...)
    {
        std::cerr << "unknown exception" << std::endl;
        return 1;
    }

    return 0;
}
