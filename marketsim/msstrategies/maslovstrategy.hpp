#ifndef MASLOVSTRATEGY_HPP
#define MASLOVSTRATEGY_HPP

#include "marketsim.hpp"

namespace marketsim
{

template <int eventspersec, int volatilityperc,int windowsize>
class generalmaslovstrategy: public teventdrivenstrategy
{
public:
       generalmaslovstrategy()
           : teventdrivenstrategy(1.0 / std::max( eventspersec, 1))
       {
           fsigma = sqrt(pow(volatilityperc / 100,2) / ( 24 * 3600* 365 ) * interval());
       }

       virtual trequest event(const tmarketinfo&, tabstime, trequestresult* last)
       {
            possiblylog("event procedure entered","by maslovstrategy");
           if(!last)
               flogfair = 2+3 * uniform();

           flogfair += w(engine()) * fsigma;
           double logprice = flogfair
               + static_cast<double>(windowsize) * (uniform() - 0.5)/ 2.0;
           bool buy = uniform() > 0.5;
           tprice lprice = static_cast<tprice>(exp(logprice) + 0.5);
           tpreorderprofile pp;
           if(buy)
           {
               // we add this condition only to avoid useless formatting in case logging
               // does not take place
               if(market()->islogging())
               {
                   std::ostringstream ls;
                   ls << "Lim. price: " << lprice;
                   possiblylog("Buy limit order requested.",ls.str());
               }
               tpreorder p(lprice,1);
               pp.B.add(p);
           }
           else
           {
               if(market()->islogging())
               {
                   std::ostringstream ls;
                   ls << "Lim. price: " << lprice;
                   possiblylog("sell limit order requested.",ls.str());
               }
               tpreorder o(lprice,1);
               pp.A.add(o);
           }
           return trequest({pp,trequest::teraserequest(false),0});
       }
private:
    double flogfair;
    double fsigma;
    std::normal_distribution<> w;
};

using maslovstrategy = generalmaslovstrategy<10,30,10>;

} // namespace
#endif // MASLOVSTRATEGY_HPP
