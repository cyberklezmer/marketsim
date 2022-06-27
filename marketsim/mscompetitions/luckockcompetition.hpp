#ifndef LUCKOCKCOMPETITION_HPP
#define LUCKOCKCOMPETITION_HPP

#include "marketsim/competition.hpp"
#include "msstrategies/luckockstrategy.hpp"
#include "msstrategies/initialstrategy.hpp"


namespace marketsim
{

template <bool chronos=true, bool logging = false>
inline void luckockcompetition(std::vector<competitorbase<chronos>*> acompetitors,
                                  twallet endowment,
                                  const tcompetitiondef& adef,
                                  std::ostream& protocol)
{
    std::vector<competitorbase<chronos>*> competitors = acompetitors;
    std::vector<twallet> endowments(acompetitors.size(),endowment);

    competitor<initialstrategy<90,110>,chronos,true> initial("initial");
    competitors.push_back(&initial);
    endowments.push_back(twallet::infinitewallet());

    competitor<cancellinguniformluckockstrategy<1,200,360,3600,false>,
                      chronos,true> cul("luckock");
    competitors.push_back(&cul);

    endowments.push_back(twallet::infinitewallet());

    competition<chronos,true,logging>(competitors,endowments,adef,protocol);
}



} // namespace



#endif // LUCKOCKCOMPETITION_HPP
