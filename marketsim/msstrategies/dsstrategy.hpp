#ifndef DSSTRATEGY_HPP
#define DSSTRATEGY_HPP

#include "marketsim.hpp"

namespace marketsim
{

template <int lambdaabs, int lambdalin, int volatilityperc>
class dsstrategy: public teventdrivenstrategy
{
public:
       dsstrategy()
           : teventdrivenstrategy(1.0 / std::max( lambdaabs, 1),true),
             flogfair
       {
           fsigma = sqrt(pow(volatilityperc / 100,2) / ( 24 * 3600* 365 ));
       }

       virtual trequest event(const tmarketinfo&, tabstime, trequestresult*)
       {
           flogfair += w(fengine) * fsigma;
           double logprice = flogfair
               + static_cast<double>(windowsize) * (uniform() - 0.5)/ 2.0;
           bool buy = uniform() > 0.5;
           tprice lprice = static_cast<tprice>(exp(logprice) + 0.5);
           tpreorderprofile pp;
           if(buy)
           {
               tpreorder p(lprice,1);
               pp.B.add(p);
           }
           else
           {
               tpreorder o(lprice,1);
               pp.A.add(o);
           }
           return trequest({pp,trequest::teraserequest(false),0});
       }
private:
    double flogfair;
    double fsigma;
    std::normal_distribution<> w;
};

};

} // namespace

#endif // DSSTRATEGY_HPP
