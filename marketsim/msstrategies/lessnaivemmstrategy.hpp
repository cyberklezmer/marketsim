#ifndef LESSNAIVEMMSTRATEGY_HPP
#define LESSNAIVEMMSTRATEGY_HPP

#include "marketsim.hpp"

namespace marketsim {

template <int volume = 1,int eventsperhour = 5*3600>
class lessnaivemmstrategy: public teventdrivenstrategy
{
public:
       lessnaivemmstrategy()
           : teventdrivenstrategy(3600.00 / eventsperhour, false)
       {
       }

       virtual trequest event(const tmarketinfo& mi, tabstime t, trequestresult* )
       {
           trequest ret;

           auto alpha = mi.alpha();
           auto beta = mi.beta();
           if(alpha == khundefprice)
               alpha = mi.lastdefineda();
           if(beta == klundefprice)
               beta = mi.lastdefinedb();

           if(alpha != khundefprice && beta != khundefprice)
           {
               if(alpha <= beta)
               {
                   if(uniform()<0.5)
                       alpha = beta+1;
                   else if(alpha > 1)
                       beta = alpha - 1;
                   else
                   {
                       beta = 1;
                       alpha = 2;
                   }
               }

               double p = (alpha + beta)/2;
               tprice c = 0;
               tprice threshold = 5*volume*p;
               if(mi.mywallet().money() > threshold)
                    c = mi.mywallet().money()-threshold;

               tprice proposeda;
               tprice proposedb;
               if(alpha - beta == 1)
               {
                   proposeda = alpha;
                   proposedb = beta;
               }
               else if(alpha - beta == 1)
               {
                   if(uniform() < 0.5)
                   {
                       proposeda = alpha;
                       proposedb = beta+1;
                   }
                   else
                   {
                       proposeda = alpha-1;
                       proposedb = beta;
                   }
               }
               else
               {
                   proposeda = alpha-1;
                   proposedb = beta+1;
               }


               tpreorderprofile pp;
               pp.B.add(tpreorder(proposedb,volume));
               pp.A.add(tpreorder(proposeda,volume));

//std::clog << t << ":" << beta << "(" << proposedb << ") - " << alpha << "(" << proposeda
//          << ") m=" << mi.mywallet().money() << " n=" << mi.mywallet().stocks() <<std::endl;
               //return ord;
               return {pp,trequest::teraserequest(true),c};
           }
           return trequest();
       }
};

}
#endif // NAIVEMMSTRATEGY_HPP
