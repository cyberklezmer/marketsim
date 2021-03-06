#ifndef NAIVEMMSTRATEGY_HPP
#define NAIVEMMSTRATEGY_HPP

#include "marketsim.hpp"

namespace marketsim {

template <int volume = 1, int eventsperhour = 3600>
class naivemmstrategy: public teventdrivenstrategy
{
public:
       naivemmstrategy(double interval=3600.0/ eventsperhour)
           : teventdrivenstrategy(interval)
       {
       }

       virtual trequest event(const tmarketinfo& info, tabstime, trequestresult* )
       {
           tprice alpha = info.alpha();
           tprice beta = info.beta();

           if(alpha != khundefprice && beta != klundefprice)
           {
               double p = (alpha + beta)/2;
               tprice c = 0;
               if(info.mywallet().money() > 5*p)
                    c = info.mywallet().money()-5*p;
               tprice mya = info.myorderprofile().a();
               tprice myb = info.myorderprofile().b();

               tprice proposeda = mya;
               if(alpha <= mya)
                   proposeda = alpha-1;

               tprice proposedb = myb;
               if(beta != klundefprice && beta >= myb)
                   proposedb = beta+1;

               if(proposeda >= proposedb)
               {
                  if(proposeda == proposedb)
                  {
                      if(uniform() < 0.5)
                          proposeda++;
                      else
                          proposedb--;
                  }
                  tpreorderprofile pp;

                  pp.B.add(tpreorder(proposedb,volume));
                  pp.A.add(tpreorder(proposeda,volume));

                  trequest ord;
                  ord.addbuylimit(proposedb, volume);
                  ord.addselllimit(proposeda, volume);
                  ord.setconsumption(c);


//cout << beta << "(" << proposedb << ") - " << alpha << "(" << proposeda << ")" << endl;
                  //return ord;
                  return {pp,trequest::teraserequest(true),c};
               }
           }
           return trequest();
       }
};

}
#endif // NAIVEMMSTRATEGY_HPP
