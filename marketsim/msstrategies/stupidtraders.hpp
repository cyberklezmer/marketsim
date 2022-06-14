#ifndef STUPIDTRADERS_HPP
#define STUPIDTRADERS_HPP

#include "marketsim.hpp"

namespace marketsim
{

template <int volume, int intervalms, bool buyer>
class stupidtrader: public teventdrivenstrategy
{
public:
       stupidtrader()
           : teventdrivenstrategy(0)
       {
       }

       virtual trequest event(const tmarketinfo& info, tabstime t, trequestresult* lrr)
       {
           if(!lrr)
               setinterval(intervalms / 1000.0);
           trequest r;

           if constexpr(buyer)
           {
               auto mymoney = info.mywallet().money();
               if(mymoney > 0)
               {
                  auto lda = info.lastdefineda();
                  tvolume buyablevolume= lda == khundefprice ? 0 : mymoney / lda;
                  tvolume v = std::min(buyablevolume,volume);
                  if(v)
                      r.addbuymarket(v);
               }
           }
           else
           {
               auto mystocks = info.mywallet().stocks();
               if(mystocks > 0)
               {
                  auto ldb = info.lastdefinedb();
                  if(ldb != klundefprice)
                  {
                      tvolume v = std::min(mystocks,volume);
                      if(v)
                          r.addsellmarket(v);
                   }
               }
           }
           return r;
       }
private:

    virtual bool acceptsdemand() const  { return buyer; }
    virtual bool acceptssupply() const  { return !buyer; }
};

}


#endif // STUPIDTRADERS_HPP
