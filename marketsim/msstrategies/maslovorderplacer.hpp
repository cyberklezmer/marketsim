#ifndef MASLOVORDERPLACER_HPP
#define MASLOVORDERPLACER_HPP

#include "marketsim.hpp"

namespace marketsim
{

class generalmaslovorderplacer: public teventdrivenstrategy
{
public:
       generalmaslovorderplacer(int volatilityperc, int windowsize, double initiallogfair)
           : teventdrivenstrategy(0),
             flogfair(initiallogfair), fwindowsize(windowsize),
             flastt(0), flastdssize(0)
       {
           fsigma = volatilityperc / 100.0;
       }

       virtual trequest event(const tmarketinfo& info, tabstime t, trequestresult* lrr)
       {
           if(!lrr)
               setinterval(market()->def().demandupdateperiod);
           tabstime dt = t - flastt;
           double delta = w(engine()) * fsigma * sqrt(1.0 / ( 24 * 3600 * 365 ) * dt);
           flogfair += delta;
           flastt = t;
           const auto& h = info.myinfo().dshistory();
           int n = h.size();
           if(n > flastdssize)
           {

               auto ds = h[flastdssize++];
               if(ds.fdemand==0 || ds.fsupply==0)
               {
                   tprice lprice = static_cast<tprice>(exp(flogfair) + 0.5)
                           + (uniform() - 0.5) * fwindowsize;
                   if(lprice < 1)
                       lprice = 1;
                   tpreorderprofile pp;
                   if(ds.fdemand != 0)
                   {
                       auto lda = info.lastdefineda();
                       tvolume buyablevolume= lda == khundefprice ? 0 : ds.fdemand / lda;
                       if(buyablevolume)
                       {
                          tpreorder p(lprice,buyablevolume);
                          pp.B.add(p);
                       }
                   }
                   else
                   {
                       if(ds.fsupply > 0 && info.lastdefinedb() != klundefprice)
                       {
                           tpreorder o(lprice,ds.fsupply);
                           pp.A.add(o);
                       }
                   }
                   return trequest({pp,trequest::teraserequest(false),0});
               }
           }
           return trequest();
       }
private:
    double flogfair;
    double fsigma;
    int fwindowsize;
    tabstime flastt;
    int flastdssize;
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

} // namespace
#endif // MASLOVORDERPLACER_HPP
