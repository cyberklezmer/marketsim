#ifndef ORIGINALCOMPETITION_HPP
#define ORIGINALCOMPETITION_HPP

#include "marketsim/competition.hpp"
#include "msstrategies/firstsecondstrategy.hpp"
#include "msstrategies/liquiditytakers.hpp"
#include "msstrategies/naivemmstrategy.hpp"

namespace marketsim
{

template <bool chronos=true, bool logging = false>
inline void originalcompetition(std::vector<competitorbase<chronos>*> acompetitors,
                                  twallet endowment,
                                  const tcompetitiondef& adef,
                                  std::ostream& protocol)
{
    std::vector<competitorbase<chronos>*> competitors = acompetitors;

    competitor<naivemmstrategy<10>,chronos> nmm("naivka");

    competitor<firstsecondstrategy<40,10>,chronos,true> fss("firstsec");

    competitor<liquiditytakers<360,5>,chronos,true> lts("lt");

    std::vector<twallet> endowments(acompetitors.size()+1,endowment);
    endowments.push_back(twallet::infinitewallet());
    endowments.push_back(twallet::infinitewallet());

    competitors.push_back(&nmm);
    competitors.push_back(&fss);
    competitors.push_back(&lts);

    competition<chronos,true,logging>(competitors,endowments,adef,protocol);
}


};

#endif // ORIGINALCOMPETITION_HPP
