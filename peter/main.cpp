#pragma warning(disable : 4996)
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
#include "msstrategies/liquiditytakers.hpp"
#include "msstrategies/firstsecondstrategy.hpp"


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
        twallet endowment(5000,100);

        tmarketdef def;

        // you can modify def, perhaps adjust logging etc

        // change accordingly but note that with chronos==true the streategies must be
        // descentants of marketsim::teventdrivenstrategy
        using testedstrategy = tadpmarketmaker;
        using secondtestedstrategy = naivemmstrategy<1>;


        enum ewhattodo { esinglerunsinglestrategy,
                         esinglerunstrategyandmaslov,
                         ecompetesinglestrategyandandmaslov,
                         ecompetetwostrategiesandandmaslov,
                         eltcompetition};

        // change accordingly
        ewhattodo whattodo = eltcompetition;// ecompetetwostrategiesandandmaslov; // ecompetesinglestrategyandandmaslov;

        switch(whattodo)
        {
        case esinglerunsinglestrategy:
            {
                competitor<testedstrategy,chronos> s;
                test<chronos,true,logging>({&s},runningtime,endowment,def);
            }
            break;
        case esinglerunstrategyandmaslov:
            {
                competitor<testedstrategy,chronos> s;
                competitor<maslovstrategy,chronos> m;
                test<chronos,true,logging>({&s,&m},runningtime,endowment,def);
            }
            break;
        case ecompetesinglestrategyandandmaslov:
            {
                competitor<testedstrategy,chronos> s;
                competitionwithmaslov<chronos,true,logging>({&s}, runningtime, endowment, def, std::clog);
            }
            break;
        case ecompetetwostrategiesandandmaslov:
            {
                competitor<testedstrategy,chronos> fs;
                competitor<secondtestedstrategy,chronos> ss;
                competitionwithmaslov<chronos,true,logging>({&fs,&ss}, runningtime, endowment, def, std::clog);
            }
            break;
        case eltcompetition:
            {
            // built in strategies

            // this strategy makes 40 random orders during the first second
            competitor<firstsecondstrategy<40,10>,chronos,true> fss;

            // this strategy simulates liquidity takers:
            // 360 times per hour on average it puts a market order with volume 5 on average
            competitor<liquiditytakers<360,5>,chronos,true> lts;


            // competing strategies

            // this strategy just wants to put orders into spread and when it
            // has more money than five times the price, it consumes. The volume of
            // the orders is always 10

            competitor<naivemmstrategy<10>,chronos> nmm;

            // our ingenious strategy
            competitor<testedstrategy,chronos> ts;

            competition<chronos,true,logging>({&fss,&lts,&nmm,&ts}, runningtime, endowment, def, std::clog);

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



