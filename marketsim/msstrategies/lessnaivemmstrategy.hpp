#ifndef LESSNAIVEMMSTRATEGY_HPP
#define LESSNAIVEMMSTRATEGY_HPP

#include "marketsim.hpp"

namespace marketsim {

template <int volume = 1,int eventsperhour = 5*3600>
class lessnaivemmstrategy: public teventdrivenstrategy
{
public:
       lessnaivemmstrategy()
           : teventdrivenstrategy(eventsperhour / 3600.00, false)
       {
       }

       virtual trequest event(const tmarketinfo& info, tabstime, trequestresult* )
       {
           tprice alpha = info.alpha();
           tprice beta = info.beta();
           if(alpha != khundefprice)
               beta = alpha/2;
           if(beta != klundefprice)
               alpha = beta * 2;


           if(alpha != khundefprice && beta != klundefprice)
           {
               double p = (alpha + beta)/2;
               tprice c = 0;
               if(info.mywallet().money() > 5*p*volume)
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
                  if(info.mywallet().stocks() > volume / 2)
                      pp.A.add(tpreorder(proposeda,
                                std::min(info.mywallet().stocks()-volume/2,volume)));
//cout << beta << "(" << proposedb << ") - " << alpha << "(" << proposeda << ")" << endl;
                  return {pp,trequest::teraserequest(true),c};
               }
           }
           return trequest();
       }
};

}
#endif // NAIVEMMSTRATEGY_HPP
