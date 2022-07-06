#ifndef LUCKOCKSTRATEGY_HPP
#define LUCKOCKSTRATEGY_HPP

#include "marketsim.hpp"
#include "msstrategies/cancellingdsstrategy.hpp"

namespace marketsim
{

template <int volume, int eventsperhour, bool random, tprice K(double), tprice L(double) >
class generalluckockstrategy: public teventdrivenstrategy
{
public:
       generalluckockstrategy() : teventdrivenstrategy(0), fpd(volume)
       {
       }

       virtual trequest event(const tmarketinfo&, tabstime, trequestresult*)
       {
           possiblylog("event procedure entered","by generalluckockstrategy");

           bool buy = uniform() < 0.5;

           tvolume v;
           if constexpr(random)
                v = fpd(engine());
           else
                v = volume;

           trequest ret;
           if(buy)
           {
               tprice lprice = K(uniform());
               if(market()->islogging())
               {
                   std::ostringstream ls;
                   ls << "Lim. price: " << lprice << " vol: " << v;
                   possiblylog("Buy limit order requested.",ls.str());
               }
               ret.addbuylimit(lprice,v);
           }
           else
           {
               tprice lprice = L(uniform());
               if(market()->islogging())
               {
                   std::ostringstream ls;
                   ls << "Lim. price: " << lprice << " vol: " << v;
                   possiblylog("sell limit order requested.",ls.str());
               }
               ret.addselllimit(lprice,v);
           }
           double constexpr eventspersec = eventsperhour / 3600.0;
           setinterval(-log(uniform()) / eventspersec );
           return ret;
       }
private:
       std::poisson_distribution<> fpd;
};

template <int maxprice>
inline tprice uniformquantile(double u) { return static_cast<tprice>(u*maxprice); }

template <int volume, int maxprice, int eventsperhour, bool random>
using uniformluckockstrategy=generalluckockstrategy<volume,eventsperhour,random,
          uniformquantile<maxprice>,uniformquantile<maxprice>>;

template <int volume, int maxprice, int meanlifesec, int eventsperhour, bool random>
using cancellinguniformluckockstrategy
  = cancellingstrategy<uniformluckockstrategy<volume, maxprice, eventsperhour, random>, meanlifesec>;

} // namespace

#endif // LUCKOCKSTRATEGY_HPP
