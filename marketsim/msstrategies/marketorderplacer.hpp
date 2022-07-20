#ifndef MARKETORDERPLACER_HPP
#define MARKETORDERPLACER_HPP

#include "marketsim.hpp"
#include "msstrategies/cancellingdsstrategy.hpp"

namespace marketsim
{

template <bool demand=true, bool supply=true>
class marketorderplacer: public tdsprocessingstrategy<demand,supply>
{
public:
     virtual trequest dsevent(const tdsevent& ds, const tmarketinfo& mi, tabstime t)
     {
          bool buy = ds.ds.d > 0;
          bool sell = ds.ds.s > 0;
          if(buy && sell)
               assert(1);

          trequest ret;
          auto lda = mi.lastdefineda();
          auto ldb = mi.lastdefinedb();
// std::cout << t << " - " << ldb << " - " << lda << std::endl;

          if(buy)
          {
              if(mi.orderbook().A.size()==0)
              {
                  if(ldb != klundefprice)
                      ret.addbuylimit(ldb,ds.ds.d);
              }
              else
                  ret.addbuymarket(ds.ds.d);
          }
          else // buy
          {
              if(mi.orderbook().B.size()==0)
              {
                  if(lda != khundefprice)
                      ret.addselllimit(lda,ds.ds.s);
              }
              else
                  ret.addsellmarket(ds.ds.s);
           }
           return ret;
       }
private:
    virtual bool acceptsdemand() const  { return true; }
    virtual bool acceptssupply() const  { return true; }
};


template <int meanlife, bool demand=true, bool supply=true>
using cancellingmarketorderplacer
 = cancellingstrategy<marketorderplacer<demand, supply>,meanlife>;

} // namespace

#endif // MARKETORDERPLACER_HPP
