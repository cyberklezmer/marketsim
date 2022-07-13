#include <vector>
#include <iostream>
#include <limits>
#include <fstream>
#include <algorithm>
#include <random>

#include "marketsim.hpp"
#include "marketsim/competition.hpp"
#include "marketsim/tests.hpp"
#include "msstrategies/adpmarketmaker.hpp"
#include "msstrategies/naivemmstrategy.hpp"
// #include "msstrategies/lessnaivemmstrategy.hpp"
#include "msstrategies/heuristicstrategy.hpp"
#include "msstrategies/trendspeculator.hpp"

#include "msstrategies/initialstrategy.hpp"
#include "msstrategies/maslovstrategy.hpp"
#include "msstrategies/luckockstrategy.hpp"

#include "torch/torch.h"
#include "config.hpp"

using namespace marketsim;

//enum zitype { emaslov, eluckock, enumzitypes };

template <bool chronos=false, bool logging = false>
std::vector<competitionresult> zicompetition(
        std::vector<competitorbase<chronos>*> acompetitors,
        competitorbase<chronos>* zicompetitor,
        twallet endowment,
        const tcompetitiondef& adef,
        std::ostream& protocol)
{
    std::vector<competitorbase<chronos>*> competitors = acompetitors;
    std::vector<twallet> endowments(acompetitors.size(),endowment);

    competitor<initialstrategy<90,110>,chronos,true> initial("initial");
    competitors.push_back(&initial);
    endowments.push_back(twallet::infinitewallet());

    competitors.push_back(zicompetitor);
    endowments.push_back(twallet::infinitewallet());

    return competition<chronos,true,logging>(competitors,endowments,adef,protocol);
}


template <bool logging = false>
void separatecompetition(std::vector<competitorbase<false>*> acompetitors,
                          std::vector<competitorbase<false>*> azistrategies,
                          const tcompetitiondef& adef,
                          twallet endowment)
{
    std::vector<statcounter> res;
    tcompetitiondef def = adef;
    for(unsigned z=0; z<azistrategies.size(); z++)
        for(unsigned c=0; c<acompetitors.size(); c++)
        {
            std::ostringstream s;
            s << azistrategies[z]->name() << "_"
              << acompetitors[c]->name() << "_";
            def.id = s.str();

            std::vector<competitorbase<false>*> arg;
            arg.push_back(acompetitors[c]);
            std::vector<competitionresult> r
                    = zicompetition<false,logging>(
                      arg,azistrategies[z],endowment,def, std::clog);
            res.push_back(r[0].consumption);
            def.seed++;
        }
    std::ostream& o = std::cout;

    o << "Results of separate competition" << std::endl;
    unsigned i=0;
    o << "zi/competitor";
    for(unsigned c=0; c<acompetitors.size(); c++)
        o << "," << acompetitors[c]->name();
    o << std::endl;
    for(unsigned z=0; z<azistrategies.size(); z++)
    {
        o << azistrategies[z]->name();
        for(unsigned c=0; c<acompetitors.size(); c++)
        {
            statcounter sc = res[i++];
            o << "," << sc.average() << "(" << sqrt(sc.averagevar()) << ")";
        }
        o << std::endl;
    }
}

int main()
{
    try
    {
        constexpr bool logging = true;

        twallet endowment(5000,100);
        tmarketdef def;

        def.loggingfilter.fprotocol = true;
        def.loggingfilter.fsettle = true;
        def.loggedstrategies.push_back(2);
        def.directlogging = true;
 //       def.demandupdateperiod = 0.1;
        tcompetitiondef cdef;
        cdef.timeofrun = 8*3600;
        cdef.samplesize = 10;
        cdef.marketdef = def;

        competitor<naivemmstrategy<10>> cm("naivka");
        competitor<heuristicstrategy<false>> h("heuristic");
        competitor<adpmarketmaker> am("adpmm");
        competitor<neuralstrategy> ns("neuronka");

        competitor<spec_neuralstrategy> nspec("neuronspeculator");
        competitor<trendspeculator> tspec("trendspeculator");


        competitor<cancellingmaslovstrategy<10,3600,360,30>> ms("maslov");
        competitor<cancellinguniformluckockstrategy<10,200,360,3600,false>> ls("luckock");

//        separatecompetition<logging>({&am,&ns,&cm,&h},{&ms,&ls},cdef,twallet(5000,100));

        separatecompetition<logging>({&nspec/*,&nspec*/},{&ms,&ls},cdef,twallet(5000,100));
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



