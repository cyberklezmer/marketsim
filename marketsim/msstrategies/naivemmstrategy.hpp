#ifndef NAIVEMMSTRATEGY_HPP
#define NAIVEMMSTRATEGY_HPP

#include "marketsim.hpp"

namespace marketsim {

class naivemmstrategy: public teventdrivenstrategy
{
public:
       naivemmstrategy(double interval=1)
           : teventdrivenstrategy(interval, false)
       {
       }

       virtual trequest event(const tmarketinfo& info, tabstime, bool)
       {
           tprice alpha = info.alpha();
           tprice beta = info.beta();


           if(alpha != khundefprice && beta != klundefprice)
           {
               double p = (alpha + beta)/2;
               tprice c = 0;
               if(info.mywallet().money() > 5*p)
                    c = info.mywallet().money()-5*p;
               tprice mya = info.myorderprofile.a();
               tprice myb = info.myorderprofile.b();

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

                  pp.B.add(tpreorder(proposedb,1));
                  pp.A.add(tpreorder(proposeda,1));

//cout << beta << "(" << proposedb << ") - " << alpha << "(" << proposeda << ")" << endl;

                  return {pp,trequest::teraserequest(true),c};
               }
           }
           return trequest();
       }
};

}
#endif // NAIVEMMSTRATEGY_HPP
