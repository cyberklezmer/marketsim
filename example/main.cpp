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
        constexpr tabstime runningtime = 1000;

        // change accordingly
        twallet endowment(5000,100);

        tmarketdef def;

        // you can modify def, perhaps adjust logging etc

        // change accordingly but note that with chronos==true the streategies must be
        // descentants of marketsim::teventdrivenstrategy
        using testedstrategy = naivemmstrategy<1>;
        using secondtestedstrategy = naivemmstrategy<1>;


        enum ewhattodo { esinglerunsinglestrategy,
                         esinglerunstrategyandmaslov,
                         ecompetesinglestrategyandandmaslov,
                         ecompetetwostrategiesandandmaslov };

        // change accordingly
        ewhattodo whattodo = ecompetetwostrategiesandandmaslov;

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
                tcompetitiondef cdef;
                cdef.timeofrun = runningtime;
                cdef.endowment = endowment;
                cdef.marketdef = def;
                competitor<testedstrategy,chronos> s;
                competitionwithmaslov<chronos,true,logging>({&s}, cdef, std::clog);
            }
            break;
        case ecompetetwostrategiesandandmaslov:
            {
                competitor<testedstrategy,chronos> fs;
                competitor<secondtestedstrategy,chronos> ss;
                tcompetitiondef cdef;
                cdef.timeofrun = runningtime;
                cdef.endowment = endowment;
                cdef.marketdef = def;                
                competitionwithmaslov<chronos,true,logging>({&fs,&ss}, cdef, std::clog);
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



