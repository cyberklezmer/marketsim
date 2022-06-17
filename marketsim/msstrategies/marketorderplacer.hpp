#ifndef MARKETORDERPLACER_HPP
#define MARKETORDERPLACER_HPP

#include "marketsim.hpp"

namespace
{

class marketorderplacer: public teventdrivenstrategy
{
public:
       marketorderplacer(): teventdrivenstrategy(0)
       {
       }

       virtual trequest event(const tmarketinfo& mi, tabstime, trequestresult* lrr)
       {
           if(!lrr)
               setinterval(market()->def().demandupdateperiod);
           trequest r;
           const twallet& w=mi.mywallet();
           auto lda = mi.lastdefineda();
           tvolume buyablevolume= lda == khundefprice ? 0 : w.money() / lda;
           bool buy = buyablevolume > 0;
           bool sell = w.stocks() > 0 && mi.lastdefinedb() != klundefprice;

           if(buy && sell)
           {
               buy = uniform() > 0.5;
               sell = !buy;
           }
           if(buy)
           {
               r.addbuymarket(buyablevolume);
               std::ostringstream s;
               s << "volume " << buyablevolume;
               possiblylog("BMO submitted",s.str());
           }
           if(sell)
           {
              r.addsellmarket(w.stocks());
              std::ostringstream s;
              s << "volume " << w.stocks();
              possiblylog("SMO submitted",s.str());
           }
           return r;
       }
private:
    virtual bool acceptsdemand() const  { return true; }
    virtual bool acceptssupply() const  { return true; }
};

} // namespace

#endif // MARKETORDERPLACER_HPP
