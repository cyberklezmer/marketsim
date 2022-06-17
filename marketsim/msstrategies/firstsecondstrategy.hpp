#ifndef FIRSTSECONDSTRATEGY_HPP
#define FIRSTSECONDSTRATEGY_HPP

#include "marketsim.hpp"

namespace marketsim
{

template <int numeventsinfirstseconds, int windowsize>
class firstsecondstrategy: public teventdrivenstrategy
{
public:
       firstsecondstrategy()
           : teventdrivenstrategy(1.0 / std::max( numeventsinfirstseconds, 1),false)
       {
       }

       virtual trequest event(const tmarketinfo&, tabstime t, trequestresult* f)
       {
           if(!f)
               flogfair = 2+3 * uniform();

           double logprice = flogfair
               + static_cast<double>(windowsize) * uniform() / 2.0;
           bool buy = uniform() > 0.5;
           tprice lprice = static_cast<tprice>(exp(logprice) + 0.5);
           tpreorderprofile pp;
           if(buy)
           {
               tpreorder p(lprice,1);
               pp.B.add(p);
           }
           else
           {
               tpreorder o(lprice,1);
               pp.A.add(o);
           }
           if(t>1)
               setinterval(std::numeric_limits<tabstime>::max());
           return trequest({pp,trequest::teraserequest(false),0});
       }
private:
    double flogfair;
};


} // namespace

#endif // FIRSTSECONDSTRATEGY_HPP
