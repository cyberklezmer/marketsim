#ifndef MASLOVCOMPETITION_HPP
#define MASLOVCOMPETITION_HPP

#include "marketsim/competition.hpp"
#include "msstrategies/maslovstrategy.hpp"

namespace marketsim
{

template <bool chronos=true, bool logging = false>
inline void maslovcompetition(std::vector<competitorbase<chronos>*> acompetitors,
                              twallet endowment,
                                  const tcompetitiondef& adef,
                                  std::ostream& protocol)
{
    std::vector<competitorbase<chronos>*> competitors = acompetitors;

    std::vector<twallet> endowments(acompetitors.size(),endowment);

    competitor<maslovstrategy,chronos,true> cm("maslov(internal)");

    endowments.push_back(twallet::infinitewallet());

    competitors.push_back(&cm);

    competition<chronos,true,logging>(competitors,endowments,adef,protocol);

}


}

#endif // MASLOVCOMPETITION_HPP
