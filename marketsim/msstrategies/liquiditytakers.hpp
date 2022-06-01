#ifndef LIQUIDITYTAKERS_HPP
#define LIQUIDITYTAKERS_HPP

#include "marketsim.hpp"

namespace marketsim
{

template <int eventsperhour, int volumemean>
class liquiditytakers: public teventdrivenstrategy
{
public:
       liquiditytakers()
           : teventdrivenstrategy(3600.0 / std::max(eventsperhour, 1),true),
             pd(volumemean)
       {
       }

       virtual trequest event(const tmarketinfo&, tabstime t, trequestresult*)
       {
           bool buy = uniform() > 0.5;
           auto volume = pd(fengine);
           trequest r;
           if(volume)
           {
               if(buy)
                   r.addsellmarket(volume);
               else
                   r.addbuymarket(volume);
           }
           return r;
       }
private:
   std::poisson_distribution<> pd;
};

} // namespace

#endif // LIQUIDITYTAKERS_HPP
