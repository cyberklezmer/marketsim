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
           trequest ret;
           if(ds.ds.d>0 || ds.ds.s>0)
           {
               tprice lprice = static_cast<tprice>(exp(flogfair) + 0.5)
                       + (uniform() - 0.5) * fwindowsize;
               if(lprice < 1)
                   lprice = 1;
               tpreorderprofile pp;
               if(ds.ds.d > 0)
                  ret.addbuylimit(lprice,ds.ds.d);
               if(ds.ds.s > 0)
                  ret.addselllimit(lprice,ds.ds.s);
           }
           return ret;
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
