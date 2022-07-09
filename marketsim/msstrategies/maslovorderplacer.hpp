#ifndef MASLOVORDERPLACER_HPP
#define MASLOVORDERPLACER_HPP

#include "marketsim.hpp"
#include "msstrategies/cancellingdsstrategy.hpp"

namespace marketsim
{

class generalmaslovorderplacer: public tdsprocessingstrategy<true,true>
{
public:
       generalmaslovorderplacer(int volatilityperc, int windowsize, double initiallogfair)
           : flogfair(initiallogfair), fwindowsize(windowsize),
             flastt(0)
       {
           fsigma = volatilityperc / 100.0;
       }

       virtual trequest dsevent(const tdsevent& ds, const tmarketinfo& info, tabstime t)
       {
           tabstime dt = t - flastt;
           double delta = w(engine()) * fsigma * sqrt(1.0 / ( 24 * 3600 * 365 ) * dt);
           flogfair += delta;
           flastt = t;
           if(ds.demand==0 || ds.supply==0)
           {
               tprice lprice = static_cast<tprice>(exp(flogfair) + 0.5)
                       + (uniform() - 0.5) * fwindowsize;
               if(lprice < 1)
                   lprice = 1;
               tpreorderprofile pp;
               if(ds.demand != 0)
               {
                   auto lda = info.lastdefineda();
                   tvolume buyablevolume= lda == khundefprice ? 0 : ds.demand / lda;
                   if(buyablevolume)
                   {
                      tpreorder p(lprice,buyablevolume);
                      pp.B.add(p);
                   }
               }
               else
               {
                   if(ds.supply > 0 && info.lastdefinedb() != klundefprice)
                   {
                       tpreorder o(lprice,ds.supply);
                       pp.A.add(o);
                   }
               }
               return trequest({pp,trequest::teraserequest(false),0});
           }
           return trequest();
       }
private:
    double flogfair;
    double fsigma;
    int fwindowsize;
    tabstime flastt;
    std::normal_distribution<> w;

    virtual bool acceptsdemand() const  { return true; }
    virtual bool acceptssupply() const  { return true; }
};

template <int initialfair, int windowsize=20, int volatilityperc=30>
class maslovorderplacer : public generalmaslovorderplacer
{
public:
    maslovorderplacer()
        : generalmaslovorderplacer(volatilityperc, windowsize,
                                   log(static_cast<double>(initialfair)))
    {}
};

template <int initialfair, int meanlife, int windowsize=20, int volatilityperc=30>
using cancellingmaslovorderplacer
 = cancellingstrategy<maslovorderplacer<initialfair,windowsize,volatilityperc>,meanlife>;

} // namespace
#endif // MASLOVORDERPLACER_HPP
