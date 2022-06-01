#ifndef PARAMETRICMM_HPP
#define PARAMETRICMM_HPP

#include "marketsim.hpp"
#include "nlopt.hpp"

namespace marketsim {

struct tpmmsetting
{
    double initlambda = 3;
    double qaverageinterval = 100;
    unsigned km = 3;
    unsigned kn = 3;
    unsigned hm = 10;
    unsigned hn = 10;
    int Delta = 3;
    tvolume maxvol = 10;
    unsigned voldelta = 1;
    tprice maxconsumption = 10000;
    unsigned cdelta = 50;
    double gamma = 0.999;
    double timeinterval = 10;
    std::vector<double> phim = std::vector<double>(km,0.1);
    std::vector<double> phin = std::vector<double>(kn,0.1);
    tprice initialspread = 100;

    unsigned numlearningpoints = km * kn * 50;
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
           approx(unsigned width, unsigned n)
               : fns(n)
           {
               double x = width + 0.5;
               for(unsigned i=0; i<fns.size(); i++)
               {
                   fns[i]=basisfn(x);
                   x += width;
               }
           }
           double operator () (double x, const std::vector<double>& thetas) const
           {
               assert(thetas.size()==fns.size());
               double y = x;
               for(unsigned i=0; i<fns.size(); i++)
                   y += thetas[i] * fns[i](x);
               return y;
           }

           double operator () (unsigned n) const
                { return operator () (static_cast<double>(n)); }
       private:
           std::vector<basisfn> fns;
       };


       struct args
       {
           tprice a;
           tprice b;
           tprice c;
           tvolume v;
           tvolume w;
           double V = std::numeric_limits<double>::quiet_NaN();
           bool isnan() const { return std::isnan(V); }
       };


       struct approxpars
       {
           std::vector<double> phim;
           std::vector<double> phin;
       };



       tgeneralpmm(const tpmmsetting &s = tpmmsetting())
           : teventdrivenstrategy(s.timeinterval, false ),
             Vn(s.hn,s.kn),
             Vm(s.hm,s.km),
             fsetting(s),
             phim(s.phim),
             phin(s.phin)
       {
       }

       struct optstate
       {
           tgeneralpmm* s;
           tprice alpha;
           tprice beta;
           std::vector<tprice> ms;
           std::vector<tvolume> ns;
           std::vector<double> Vs;
       };

       static double obj(const std::vector<double> &v, std::vector<double> &, void* f_data)
       {
           const optstate& od = *(static_cast<optstate*>(f_data));
           std::vector<double> pm;
           unsigned i=0;
           for(; i<od.s->fsetting.km; i++)
               pm.push_back(v[i]);

           std::vector<double> pn;
           for(unsigned j=0; j<od.s->fsetting.kn; j++,i++)
               pn.push_back(v[i]);

           double sum = 0;
           for(unsigned i=0; i<od.ms.size(); i++)
           {
               double V = od.s->V(od.ms[i],od.ns[i],od.alpha,od.beta,{pm,pn});
               double dif = od.Vs[i] -V;
               sum += dif * dif;
           }
//static int run=0;
//std::cout << run++ << " - " << sum << std::endl;
           return sum;
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


           double aint = std::min(tnow,fsetting.qaverageinterval);
           double lambda;
           if(aint < fsetting.qaverageinterval)
               lambda = fsetting.initlambda;
           else
           {
               tvolume sumq = info.history().sumq(tnow-aint,tnow);
               lambda = std::max(1.0, sumq / aint / 2.0)*deltatau();
           }
std::cout << "lambda=" << lambda << std::endl;

if(0)
{
            optstate foptstate;
            foptstate.s = this;
            for(unsigned i=0; i<fsetting.numlearningpoints; i++)
            {
//std::cout << "Eval " << i << std::endl;
                double scale = 2* uniform();
                foptstate.alpha = alpha * scale;
                foptstate.beta = beta * scale;
                tprice m = uniform() * fsetting.hm*fsetting.km;
                tprice n = uniform() * fsetting.hn*fsetting.kn;
                args x=evaluateV(info,m,n,alpha,beta,lambda * 2 * uniform(),{phim,phin});
                if(!x.isnan())
                {
                    foptstate.Vs.push_back(x.V);
                    foptstate.ms.push_back(m);
                    foptstate.ns.push_back(n);
                }
            }


           nlopt::opt o(nlopt::LN_COBYLA,phim.size()+phin.size());
           o.set_min_objective(obj, &foptstate);
           o.set_ftol_abs(0.01);
           std::vector<double> v= phim;
           v.insert( v.end(), phin.begin(), phin.end());
           o.set_lower_bounds(std::vector<double>(v.size(),0));

           nlopt::result oresult;
           bool useit = false;
           double ovalue;
           try
           {
               oresult = o.optimize(v, ovalue);
std::cout << "Successfully optimized " << std::endl;

                   useit = true;
           }
           catch(nlopt::roundoff_limited)
           {
               std::clog << "Roundoff limited " << std::endl;
           }
           catch(std::exception &e)
           {
               std::clog << "Nlopt throwed exception: " << e.what() << std::endl;
           }

           catch(...)
           {
               std::clog << "Nlopt throwed exception, res=" << oresult << std::endl;
           }
std::cout << "optvalue=" << ovalue << ",";
           if(useit)
           {
               unsigned i=0;
               for(; i<phim.size(); i++)
               {
std::cout << v[i] << ",";
                    phim[i] = v[i];
               }
               for(unsigned j=0; j<phin.size(); j++,i++)
               {
std::cout << v[i] << ",";
                   phin[j] = v[i];
               }
           }
std::cout << std::endl;
}

           tprice m = info.mywallet().money();
           tvolume n = info.mywallet().stocks();

           args opt=evaluateV(info,m,n,alpha,beta,lambda,{phim,phin});

           if(opt.isnan())
               return trequest();
           else
           {
               std::cout << "t=" << tnow << " m=" << m << " n=" << n
                  << " alpha=" << alpha << " beta=" << beta
                  << " c=" << opt.c << " a=" << opt.a
                  << " b=" << opt.b  << " v=" << opt.v  << " w=" << opt.w << std::endl;

               /*                if(aint > 0 && sumq > fsetting.lambda / 2)
                    {
                        double aq = sumq / aint;
                        tabstime newint = 2.0 * fsetting.lambda / aq;
                        setinterval(newint);
                    }*/


               lasta = opt.a;
               lastb = opt.b;
               lastv = opt.v;
               lastw = opt.w;

               tpreorderprofile pp;

               pp.A.add(tpreorder(opt.a,opt.v));
               pp.B.add(tpreorder(opt.b,opt.w));
               return {pp,trequest::teraserequest(true),opt.c};
           }

       }
private:
       double V(unsigned m, unsigned n, double a, double b, const approxpars& ap) const
       {
           assert(m>=0);
           assert(n>=0);

           return Vm(m / b,ap.phim) * b + a * Vn(n,ap.phin);
       }

       double deltatau() const { return interval(); }

       approx Vn;
       approx Vm;
       tpmmsetting fsetting;

       // state variables:

     args evaluateV(const tmarketinfo& info, tprice m, tvolume n, tprice alpha, tprice beta, double lambda,
                    const approxpars& ap)
     {
        args opt;

        double discount = pow(fsetting.gamma,deltatau());

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
                                 assert(c>=0);
                                 double p = exp(-lambda);
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
                                     p *= lambda / i;
                                     sump += p;
                                 }


                                 p = exp(-lambda);
                                 sump = p;
                                 std::vector<double> LD(1,0);
                                 for(int i=0;;)
                                 {
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
                                     p *= lambda / i;
                                     sump += p;
                                 }

                                 double EV = 0;

                                 double checkp = 0;
                                 for(tvolume C=0; C< LC.size(); C++)
                                     for(tvolume D=0; D< LD.size(); D++)
                                     {
                                         double prob = LC[C] * LD[D];
                                         EV += prob * V(m-c-D*b+C*a,n+D-C,a,b,ap);
                                         checkp += prob;
                                     }

                                 assert(fabs(checkp-1) < 0.0000000000001);

                                 double thisV = c + discount * EV;
        // std::cout << "   a=" << a << " b=" << b << " c=" << c << " v=" << v << " w=" << w << " disc=" << discount << " V=" << thisV;
                                 if(std::isnan(opt.V) || thisV > opt.V)
                                 {
                                     opt.a = a;
                                     opt.b = b;
                                     opt.c = c;
                                     opt.v = v;
                                     opt.w = w;
                                     opt.V = thisV;
                                 }
        // std::cout <<std::endl;
                             }

                 }


        return opt;
    }

     tprice lasta = khundefprice;
     tprice lastb = klundefprice;
     tvolume lastv = 0;
     tvolume lastw = 0;

     std::vector<double> phin;
     std::vector<double> phim;
};


} // namespace

#endif // PARAMETRICMM_HPP
