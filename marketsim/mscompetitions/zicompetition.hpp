#ifndef ZICOMPETITION_HPP
#define ZICOMPETITION_HPP

#include "marketsim/competition.hpp"
#include "msstrategies/initialstrategy.hpp"

namespace marketsim
{

template <bool chronos=false, bool logging = false, typename D=tnodemandsupply>
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

    return competition<chronos,true,logging,D>(competitors,endowments,adef,protocol);
}

}

#endif // ZICOMPETITION_HPP
