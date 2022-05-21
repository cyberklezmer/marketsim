#ifndef PARAMETRICMM_HPP
#define PARAMETRICMM_HPP

#include "marketsim.hpp"

namespace marketsim {

class generalpmm : public teventdrivenstrategy
{
public:

       class basisfn
       {
       public:
           basisfn(double kink = 0) : fkink(kink) {}
           double operator ()(double x) const { return x < fkink ? 0 : (x-fkink);}
       private:
           double fkink;
       };

       class approx
       {
       public:
           approx(std::vector<double> thetas, unsigned width, double gamma)
               : fthetas(thetas), fns(thetas.size()), fgamma(gamma)
           {
               double x = width + 0.5;
               for(unsigned i=0; i<fns.size(); i++)
               {
                   fns[i]=basisfn(x);
                   x += width;
               }
           }
           double operator () (double x)
           {
               double y = fgamma * x;
               for(unsigned i=0; i<fns.size(); i++)
                   y += fthetas[i] * fns[i](x);
           }
       private:
           std::vector<double> fthetas;
           std::vector<basisfn> fns;
           double fgamma;
       };

       struct setting
       {
           unsigned mbins = 3;
           unsigned nbins = 3;
           unsigned mbinwidth = 5;
           unsigned nbinwidth = 5;
           double gamma = 0.999;
           double initinterval = 1;
           double inittheta = 1.0;
       };


       generalpmm(const setting& s)
           : teventdrivenstrategy(s.initinterval, false ),
             Vn(std::vector<double>(s.nbins,s.inittheta),s.nbinwidth,s.gamma),
             Vm(std::vector<double>(s.mbins,s.inittheta),s.mbinwidth,s.gamma)
       {
       }

       virtual trequest event(const tmarketinfo& info, tabstime, trequestresult* )
       {
           tprice alpha = info.alpha();
           tprice beta = info.beta();
           if(alpha == khundefprice)
               alpha = lasta;
           if(beta == klundefprice)
               beta = lastb;
           if(alpha == khundefprice && beta == klundefprice)
           {
               tprice initial =
           }


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
//cout << beta << "(" << proposedb << ") - " << alpha << "(" << proposeda << ")" << endl;
                  return {pp,trequest::teraserequest(true),c};
               }
           }

           return trequest();
       }
private:
       approx Vn;
       approx Vm;
       tprice lasta = khundefprice;
       tprice lastb = klundefprice;
};

} // namespace

#endif // PARAMETRICMM_HPP
