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
          bool buy = ds.demand > 0;
          bool sell = ds.supply > 0;
          if(buy && sell)
               assert(1);

          trequest ret;
          auto lda = mi.lastdefineda();
          auto ldb = mi.lastdefinedb();
// std::cout << t << " - " << ldb << " - " << lda << std::endl;

          if(buy)
          {
              const auto& A = mi.orderbook().A;
              tprice lp = klundefprice;
              tprice cost = 0;
              tvolume av = 0;
              if(A.size()==0)
              {
                  if(ldb != klundefprice)
                  {
                      tvolume ov = ds.demand / ldb + 1;
                      ret.addbuylimit(ldb,ov);
                  }
                  // else
                  // std::cout << "ldb == klundefprice" << std::endl;
              }
              else
              {
                  for(unsigned i=0; i<A.size(); i++)
                  {
                      av += A[i].volume;
                      cost += A[i].volume * A[i].price;
                      lp = A[i].price;
                      if(cost >= ds.demand)
                          break;
                  }
                  tprice remaining = ds.demand - cost;
                  // we add one as maybe he will have spare money - if not, thn only the
                  // order will be shortened
                  tvolume ov = remaining <= 0 ? av : av + (remaining/lp) + 1;
                  ret.addbuylimit(lp,ov);
              }
           }
          else // buy
          {
              const auto& B = mi.orderbook().B;
              tprice lp = khundefprice;
              tvolume av = 0;
              if(B.size()==0)
              {
                  if(lda != khundefprice)
                  {
                      ret.addselllimit(lda,ds.supply);
                  }
                //  else
                //    std::cout << "lda == khundefprice" << std::endl;

              }
              else
              {
                  for(unsigned i=0; i<B.size(); i++)
                  {
                      av += B[i].volume;
                      lp = B[i].price;
                      if(av >= ds.supply)
                          break;
                  }
                  tvolume ov = ds.supply;
                  ret.addselllimit(lp,ov);
              }
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
