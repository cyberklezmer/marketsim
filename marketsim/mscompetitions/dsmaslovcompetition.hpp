#ifndef DSMASLOVCOMPETITION_HPP
#define DSMASLOVCOMPETITION_HPP

#include "msstrategies/maslovorderplacer.hpp"
#include "msstrategies/initialstrategy.hpp"
#include "msdss/fairpriceds.hpp"

namespace marketsim
{

template <bool chronos=true, bool logging = false, bool cancelling = true>
inline void dsmaslovcompetition(std::vector<competitorbase<chronos>*> acompetitors,
                                  twallet endowment,
                                  const tcompetitiondef& adef,
                                  std::ostream& protocol)
{
    std::vector<competitorbase<chronos>*> competitors = acompetitors;
    std::vector<twallet> endowments(acompetitors.size(),endowment);

    competitor<initialstrategy<90,100>,chronos,true> initial("initial");
    competitors.push_back(&initial);
    endowments.push_back(twallet::infinitewallet());

    competitor<maslovorderplacer<100>,chronos,true> op("orderplacer");
    competitor<cancellingmaslovorderplacer<100,100>,chronos,true> cop("corderplacer");

    if(cancelling)
        competitors.push_back(&cop);
    else
        competitors.push_back(&op);

    endowments.push_back(twallet::emptywallet());

    competition<chronos,true,logging,fairpriceds<3600,10>>(competitors,endowments,adef,protocol);
}



} // namespace

#endif // DSMASLOVCOMPETITION_HPP
