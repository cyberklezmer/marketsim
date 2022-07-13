#ifndef MASLOVSTRATEGY_HPP
#define MASLOVSTRATEGY_HPP

#include "marketsim.hpp"
#include "msstrategies/cancellingdsstrategy.hpp"


namespace marketsim
{

template <int volume, int eventsperhour,int windowsize>
class generalmaslovstrategy: public teventdrivenstrategy
{
public:
       generalmaslovstrategy()
           : teventdrivenstrategy(0),fpd(volume)
       {
       }

       virtual trequest event(const tmarketinfo& mi, tabstime, trequestresult*)
       {
            possiblylog("event procedure entered","by maslovstrategy");

            double constexpr eventspersec = eventsperhour / 3600.0;
            setinterval(-log(uniform()) / eventspersec );

            double ldp = mi.lastdefinedp();
            if(!isnan(ldp))
            {

                bool buy = uniform() > 0.5;
                double offset = (uniform() - 0.5) * windowsize;

                tprice lprice = std::max(1,static_cast<tprice>(ldp + offset + 0.5));

                tvolume v = fpd(engine());
                tpreorderprofile pp;
                if(buy)
                {
                   if(market()->islogging())
                   {
                      std::ostringstream ls;
                      ls << "Lim. price: " << lprice;
                      possiblylog("Buy limit order requested.",ls.str());
                   }
                   tpreorder p(lprice,v);
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
                   tpreorder o(lprice,v);
                   pp.A.add(o);
               }
               return trequest({pp,trequest::teraserequest(false),0});
           }
           else
               return trequest();
       }
private:
       std::poisson_distribution<> fpd;
};

using maslovstrategy = generalmaslovstrategy<10,3600,10>;

template <int volume, int eventsperhour, int meanlifesec, int windowsize>
using cancellingmaslovstrategy = cancellingstrategy<generalmaslovstrategy<volume, eventsperhour, windowsize>, meanlifesec>;

} // namespace
#endif // MASLOVSTRATEGY_HPP
