#ifndef PARAMETRICMM_HPP
#define PARAMETRICMM_HPP

#include "marketsim.hpp"

namespace marketsim {

struct tpmmsetting
{
    double lambda = 3;
    double qaverageinterval = 100;

    unsigned km = 3;
    unsigned kn = 3;
    unsigned hm = 50;
    unsigned hn = 5;
    int Delta = 3;
    tvolume maxvol = 2;
    unsigned voldelta = 1;
    tprice maxconsumption = 100;
    unsigned cdelta = 20;
    double gamma = 0.999;
    double notradediscount = 0.99;
    double initinterval = 10;
    std::vector<double> phin = std::vector<double>(1.0,kn);
    std::vector<double> phim = std::vector<double>(1.0,km);
    tprice initialspread = 100;
};



class tgeneralpmm : public teventdrivenstrategy
{
public:

       class basisfn
       {
       public:
           basisfn(double kink = 0) : fkink(kink) {}
           double operator ()(double x) const { return x < fkink ? x : fkink;}
       private:
           double fkink;
       };

       class approx
       {
       public:
           approx(std::vector<double> thetas, unsigned width)
               : fthetas(thetas), fns(thetas.size())
           {
               double x = width + 0.5;
               for(unsigned i=0; i<fns.size(); i++)
               {
                   fns[i]=basisfn(x);
                   x += width;
               }
           }
           double operator () (double x) const
           {
               double y = x;
               for(unsigned i=0; i<fns.size(); i++)
                   y += fthetas[i] * fns[i](x);
               return y;
           }

           double operator () (unsigned n) const
                { return operator () (static_cast<double>(n)); }
       private:
           std::vector<double> fthetas;
           std::vector<basisfn> fns;
       };


       tgeneralpmm(const tpmmsetting &s = tpmmsetting())
           : teventdrivenstrategy(s.initinterval, false ),
             Vn(s.phin,s.hn),
             Vm(s.phim,s.hm),
             fsetting(s)
       {
       }

       virtual trequest event(const tmarketinfo& info, tabstime tnow, trequestresult* )
       {
           tprice alpha = info.alpha();
           tprice beta = info.beta();
           if(alpha == khundefprice)
               alpha = lasta;
           if(beta == klundefprice)
               beta = lastb;
           if(alpha == khundefprice && beta == klundefprice)
           {
               double dt = 1;
               for(tabstime t = tnow;;t -= dt, dt*=2)
               {
                    bool end = false;
                    if(t<0)
                    {
                        t = 0;
                        end = true;
                    }
                    beta = info.history().b(t);
                    alpha = info.history().a(t);
                    if(alpha != khundefprice || beta != klundefprice || end)
                        break;
               }
           }
           if(alpha == khundefprice && beta == klundefprice)
           {
               beta = 1;
               alpha = beta + fsetting.initialspread;
           }
           // now at least one of alpha or beta is defined;
           if(beta == khundefprice)
           {
               beta  = std::max(1, alpha - fsetting.initialspread);
               if(alpha == beta)
                   alpha++;
           }
           else if(alpha == klundefprice)
               alpha = beta + fsetting.initialspread;

           // now both alpha and beta are defined

           assert(alpha != khundefprice);
           assert(beta != klundefprice);

           tprice m = info.mywallet().money();
           tvolume n = info.mywallet().stocks();
           tprice opta, optb, optc;
           tvolume optv, optw;
           double maxV = -1;

           double discount = pow(fsetting.gamma,-deltatau());


           for(tprice a=std::max(alpha-1,beta+1); a <= alpha+fsetting.Delta; a++)
                for(tprice b=std::max(beta-fsetting.Delta,1); b< beta+1; b++)
                    if(a > b)
                    {
                        // compute pi_b
                        tvolume pib = 0;
                        auto B = info.orderbook().B;

                        for(unsigned i=0; i<B.size(); i++)
                        {
                            auto o = B[i];
                            if(o.price < b)
                                break;
                            bool my = o.owner == info.myindex();
                            if(my)
                            {
                                if(o.price==b)
                                    break; // we neglect the fact that the new volume could be higher
                                           // so the excess volume will stand at the end of the qeueue
                                else
                                    continue;  // this order will be withdrawn
                            }
                            pib += o.volume;
                        }

                        tvolume pia = 0;
                        auto A = info.orderbook().A;

                        for(unsigned i=0; i<A.size(); i++)
                        {
                            auto o = A[i];
                            if(o.price > a)
                                break;
                            bool my = o.owner == info.myindex();
                            if(my)
                            {
                                if(o.price==a)
                                    break; // we neglect the fact that the new volume could be higher
                                           // so the excess volume will stand at the end of the qeueue
                                else
                                    continue;  // this order will be withdrawn
                            }
                            pia += o.volume;
                        }
                        tvolume purchasablevol = m / b;
                        for(tvolume v = 1;
                            v <= std::min(fsetting.maxvol,n);
                            v += fsetting.voldelta)
                            for(tvolume w = 1;
                                w <= std::min(fsetting.maxvol,purchasablevol);
                                w += fsetting.voldelta)
                                for(tprice c=0;
                                    c < std::min(fsetting.maxconsumption, m - w*b);
                                    c += fsetting.cdelta)
                                {
std::cout << "             a=" << a << " b=" << b << " c=" << c << " v=" << v << " w=" << w << " maxV=" << maxV <<std::endl;

                                    double p = exp(-fsetting.lambda);
                                    double sump = p;
                                    std::vector<double> LC(1,0);
                                    for(int i=0;;)
                                    {
                                        if(i<=pia)
                                            LC[0] += p;
                                        else
                                            LC.push_back(p);

                                        if(i==pia+v)
                                        {
                                            LC[v] += 1-sump;
                                            break;
                                        }
                                        i++;
                                        p *= fsetting.lambda / i;
                                        sump += p;
                                    }


                                    p = exp(-fsetting.lambda);
                                    sump = p;
                                    std::vector<double> LD(1,0);
                                    for(int i=0;;)
                                    {
//std::cout << "i=" << i << " p=" << p << std::endl;
                                        if(i<=pib)
                                            LD[0] += p;
                                        else
                                            LD.push_back(p);

                                        if(i==pib+w)
                                        {
                                            LD[w] += 1-sump;
                                            break;
                                        }
                                        i++;
                                        p *= fsetting.lambda / i;
                                        sump += p;
                                    }

                                    double EV = 0;

                                    double checkp = 0;
                                    for(tvolume C=0; C< LC.size(); C++)
                                        for(tvolume D=0; D< LD.size(); D++)
                                        {
                                            double prob = LC[C] * LD[D];
                                            EV += prob * V(n-c-D*b+C*a,n+D-C,
                                                           b * fsetting.notradediscount);
                                            checkp += prob;
                                        }
                                    assert(fabs(checkp-1) < 0.0000000000001);

                                    double thisV = c + discount * EV;
                                    if(thisV > maxV)
                                    {
                                        opta = a;
                                        optb = b;
                                        optc = c;
                                        optv = v;
                                        optw = w;
                                        maxV = thisV;
std::cout << "       ->>>>a=" << a << " b=" << b << " c=" << c << " v=" << v << " w=" << w << " m=" << maxV <<std::endl;

                                    }
                                }

                    }

            tpreorderprofile pp;


            lasta = opta;
            lastb = optb;
            lastv = optv;
            lastw = optw;


std::cout << "t=" << tnow
          << " alpha=" << alpha << " beta=" << beta
          << " c=" << optc << " a=" << opta
          << " b=" << optb  << " v=" << optv  << " w=" << optw << std::endl;

            double aint = std::min(tnow,fsetting.qaverageinterval);
            tvolume sumq = info.history().sumq(tnow-aint,tnow);
            if(aint > 0 && sumq > fsetting.lambda / 2)
            {
                double aq = sumq / aint;
                tabstime newint = 2.0 * fsetting.lambda / aq;
                setinterval(newint);
            }

            pp.A.add(tpreorder(opta,optv));
            pp.B.add(tpreorder(optb,optw));
            return {pp,trequest::teraserequest(true),optc};

       }
private:
       double V(unsigned m, unsigned n, double disc) const
       {
           assert(m>=0);
           assert(n>=0);
           return Vm(m) + disc * Vn(n);
       }

       double deltatau() const { return interval(); }

       approx Vn;
       approx Vm;
       tpmmsetting fsetting;

       // state variables:

       tprice lasta = khundefprice;
       tprice lastb = klundefprice;
       tvolume lastv = 0;
       tvolume lastw = 0;
};

} // namespace

#endif // PARAMETRICMM_HPP
