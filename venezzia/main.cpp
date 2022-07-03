#include <vector>
#include <iostream>
#include <limits>
#include <fstream>
#include <algorithm>
#include <random>

#include "marketsim.hpp"
#include "msstrategies/marketorderplacer.hpp"
#include "marketsim/competition.hpp"
#include "marketsim/tests.hpp"
#include "msstrategies/tadpmarketmaker.hpp"
#include "msstrategies/naivemmstrategy.hpp"
#include "msstrategies/lessnaivemmstrategy.hpp"
#include "msstrategies/firstsecondstrategy.hpp"
#include "msstrategies/liquiditytakers.hpp"
#include "msstrategies/parametricmm.hpp"
#include "msstrategies/maslovorderplacer.hpp"
#include "msstrategies/initialstrategy.hpp"
#include "msstrategies/marketorderplacer.hpp"
#include "msdss/rawds.hpp"
#include "mscompetitions/luckockcompetition.hpp"
#include "mscompetitions/dsmaslovcompetition.hpp"

using namespace marketsim;


struct tvalueiterationmarketmakersetting
{
    tabstime interval = 0.375767;
    double lambda = 0; //    0.5;
    double alpha = 0.05;
    double initconcavity = 0.1;
    double qaverageinterval = 100;
    double gamma = 0.999;
    tprice Delta = 1;
    tvolume maxvol = 10;
    unsigned voldelta = 2;
    tprice maxconsumption = 50;
    int safetyfactor = 1;

    unsigned cdelta = 50;
    int mnodes = 5;
    int nnodes = 5;
    int mdelta = 1000;
    int ndelta = 100;
};


tvalueiterationmarketmakersetting gs;



class gridapproxfn
{
public:
    gridapproxfn(int amnodes, int annodes, int amdelta, int andelta)
          : fF(amnodes,std::vector<double>(annodes)),
            mnodes(amnodes), nnodes(annodes),mdelta(amdelta), ndelta(andelta)
    {
        assert(amnodes > 1);
        assert(annodes > 1);
    }

    void setnode(unsigned m, unsigned n, double v)
    {
        assert(m<fF.size());
        assert(n<fF[0].size());
        fF[m][n] = v;
    }
    int getmnodes() const { return fF.size(); }
    int getnnodes() const { return fF[0].size(); }
    int getmdelta() const { return mdelta; }
    int getndelta() const { return ndelta; }

    double operator () (double m, double n)
    {
        assert(m >=0);
        assert(n >=0);


        int i = std::min(static_cast<int>(m / mdelta),mnodes - 2);
        int j = std::min(static_cast<int>(n / ndelta),nnodes - 2);
        double derivm = (fF[i+1][j]-fF[i][j])/mdelta;
        double derivn = (fF[i][j+1]-fF[i][j])/ndelta;
        double dm = m - i * mdelta;
        double dn = n - j * ndelta;

        return fF[i][j]+dm*derivm + dn*derivn;
    }

    struct nargs
    {
        int istart;
        int jstart;
        int iend;
        int jend;
        bool isvalid() const { return istart>=0 && jstart>=0; }
    };

    nargs getnearest(double m, double n)
    {
        int i = std::min(static_cast<int>(m / mdelta),mnodes - 1);
        int j = std::min(static_cast<int>(n / ndelta),nnodes - 1);
        double dm = m - i * mdelta;
        double dn = n - j * ndelta;
        int di;
        if(i==mnodes - 1 || dm < mdelta /2.0)
            di = i;
        else
            di = i+1;
        int dj;
        if(j==nnodes - 1 || dn < ndelta /2.0)
            dj = j;
        else
            dj = j+1;
        return { di,dj,di,di };
    }

    nargs getrectangle(double m, double n)
    {
        int i = static_cast<int>(m / mdelta);
        if(i>= mnodes - 1)
            i=-1;
        int j = static_cast<int>(n / ndelta);
        if(j >= nnodes - 1)
            j=-1;
        return {i,j,i+1,j+1};
    }
private:
    std::vector<std::vector<double>>fF;
    const int mnodes;
    const int nnodes;
    const int mdelta;
    const int ndelta;
};


inline double E(std::vector<double> d)
{
    double sum = 0;
    for(unsigned i=1; i<d.size(); i++)
        sum += i * d[i];
    return sum;
}

class valueiterationmarketmaker : public teventdrivenstrategy
{
public:

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


    valueiterationmarketmaker(const tvalueiterationmarketmakersetting& s
                              = gs ) :
        teventdrivenstrategy(s.interval),
        finitialized(false), fsetting(s), V(s.mnodes,s.nnodes,s.mdelta,s.ndelta)
    {}



    virtual trequest event(const tmarketinfo& mi, tabstime tnow, trequestresult*)
    {
        double ldp = mi.lastdefinedp();
        if(!finitialized && !isnan(ldp))
        {

            int nm = V.getmnodes();
            assert(nm>1);
            std::vector<double> Vm(nm);
            Vm[0] = 0;
            double s=0;
            int cmult = (nm-2);
            for(int i=1; ; i++)
            {
                double x = V.getmdelta() * (1+cmult * fsetting.initconcavity);
                s += x;
                Vm[i] = s;
                if(i==nm-1)
                    break;
                cmult--;
            }
            assert(cmult == 0);

            int nn = V.getnnodes();
            assert(nn>1);
            std::vector<double> Vn(nn);
            Vn[0] = 0;
            s=0;
            cmult = (nn-2);
            for(int i=1; ; i++)
            {
                double x = ldp * V.getndelta() * (1+cmult * fsetting.initconcavity);
                s += x;
                Vn[i] = s;
                if(i==nn-1)
                    break;
                cmult--;
            }
            assert(cmult == 0);
            for(int i=0;i<nm; i++)
                for(int j=0;j<nn; j++)
                    V.setnode(i,j,/* (i==0 || j==0) ? 0 : */(Vm[i]+Vn[j]));
            finitialized = true;

        }
        if(finitialized)
        {
            tprice alpha = mi.alpha();
            tprice beta = mi.beta();
            if(alpha == khundefprice)
                alpha = lasta;
            if(beta == klundefprice)
                beta = lastb;
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

                assert(alpha != khundefprice);
                assert(beta != klundefprice);

                double aint = std::min(tnow,fsetting.qaverageinterval);
                tvolume sumq = mi.history().sumq(tnow-aint,tnow);
                double lambda = std::max(1.0, sumq / aint / 2.0)*interval();

                if(lambda > 0)
                {
                    tprice m = mi.mywallet().money();
                    tvolume n = mi.mywallet().stocks();

                    std::ostringstream s;
                    s << "t=" << tnow << " m=" << m << " n=" << n
                      << " beta=" << beta << " alpha=" << alpha
                      << " lambda=" << lambda
                      << " Bvol=" << mi.orderbook().B.volume()
                         << ", Avol=" << mi.orderbook().A.volume();
                    possiblylog("Calling evaluateV:",s.str());
                    args opt=evaluateV(mi,m,n,alpha,beta,lambda);

                    if(opt.isnan())
                        return trequest();
                    else
                    {
                        std::ostringstream s;
                        s <<  "c=" << opt.c << " a=" << opt.a
                           << " b=" << opt.b  << " v=" << opt.v  << " w=" << opt.w;

                        possiblylog("about to apply", s.str());

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

/*                        static unsigned cntr = 0;

                        if(cntr++ % 100 ==0)
                        {
                            for(unsigned m=0; m<V.getmnodes(); m++)
                            {
                                for(unsigned n=0; n<V.getnnodes(); n++)
                                    std::clog << V(m*V.getmdelta(),n*V.getndelta()) << ",";
                                std::clog << std::endl;
                            }
                            std::clog << std::endl << "beta=" << beta << " alpha=" << alpha << std::endl << std::endl;
                        } */

                        startlearning();
                        // auto a = V.getrectangle(m,n);
                        gridapproxfn::nargs a = {0,0,V.getmnodes()-1,V.getnnodes()-1};
if(0)                        if(a.isvalid())
                        {
                            std::ostringstream s;
                            s << "m=" << m << "(" << a.istart << ") n=" << n << "(" << a.jstart << ") ";
                            std::vector<std::vector<double>> nv(a.iend-a.istart+1,
                                                                std::vector<double>(a.jend-a.jstart+1));
                            bool valid = true;
                            for(int i=a.istart; i<a.iend && valid; i++)
                            {
                                for(int j=a.jstart; j<=a.jend; j++)
                                {
                                    args opt=evaluateV(mi,i*V.getmdelta(),
                                                       j*V.getndelta(),alpha,beta,lambda);
                                    if(opt.isnan())
                                    {
                                        valid= false;
                                        break;
                                    }
                                    s << V(i*V.getmdelta(),j*V.getndelta()) << "->" << opt.V << ", ";
                                    nv[i-a.istart][j-a.jstart] = opt.V;;
                                }
                            }

                            if(valid)
                            {
                                // last part has to have derivative one
                                for(int j=a.jstart; j<=a.jend; j++)
                                {
                                    nv[a.iend-a.istart][j-a.jstart]
                                            = nv[a.iend-a.istart-1][j-a.jstart]
                                            + V.getmdelta();
                                }

                                for(int i=a.istart; i<=a.iend && valid; i++)
                                    for(int j=a.jstart; j<=a.jend; j++)
                                        V.setnode(i,j,nv[i-a.istart][j-a.jstart]);

                                possiblylog("V changed",s.str());
                            }

                        }
                        stoplearning();
                        tpreorderprofile pp;

                        pp.A.add(tpreorder(opt.a,opt.v));
                        pp.B.add(tpreorder(opt.b,opt.w));
                        return {pp,trequest::teraserequest(true),opt.c};
                     }
                }
            }
        }
        return trequest();
    }

private:

    args evaluateV(const tmarketinfo& info, tprice m, tvolume n, tprice alpha, tprice beta, double lambda)
    {
       args opt;
double altEV;
       double discount = pow(fsetting.gamma,interval());
       for(tprice a=std::max(alpha-1,beta+1); a <= alpha+fsetting.Delta; a++)
            for(tprice b=std::max(beta-fsetting.Delta,1); b<= beta+1; b++)
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
                    for(tvolume v = 0;
                        v <= std::min(fsetting.maxvol,n);
                        v += fsetting.voldelta)
                        for(tvolume w = 0;
                            w <= std::min(fsetting.maxvol,purchasablevol);
                            w += fsetting.voldelta)
//                            for(tprice c=0;
//                                c <= std::max(0,std::min(fsetting.maxconsumption,
//                                              m - w*b - fsetting.safetyfactor * fsetting.maxvol * alpha ));
//                                c += fsetting.cdelta)

                            {
                                tprice c = std::max(0,std::min(fsetting.maxconsumption,
                                              m - w*b - fsetting.safetyfactor * fsetting.maxvol * alpha)) ;

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
                                finitedistribution<double> dist;

//                                double checkp = 0;
                                for(tvolume C=0; C< LC.size(); C++)
                                    for(tvolume D=0; D< LD.size(); D++)
                                    {
                                        double prob = LC[C] * LD[D];
                                        double v =
                                                //V(m-c-D*b+C*a,n+D-C);
-c-D*b+C*a + (D-C) *(alpha+beta)/2.0;
                                        EV += prob * v;
                                        dist.add(prob,v);
//                                        checkp += prob;
                                    }


                                assert(dist.check());
//double thisVold = c + discount * EV;
                                double thisV = c + discount * dist.meanCVaR(fsetting.lambda,fsetting.alpha);
//assert(fabs(thisVold-thisV) < 0.00001);

//double thisV = c==0 ? EV : -100000000;
       // std::cout << "   a=" << a << " b=" << b << " c=" << c << " v=" << v << " w=" << w << " disc=" << discount << " V=" << thisV;
                                if(std::isnan(opt.V) || thisV > opt.V)
                                {
                                    opt.a = a;
                                    opt.b = b;
                                    opt.c = c;
                                    opt.v = v;
                                    opt.w = w;
                                    opt.V = thisV;
altEV = V(m,n)-E(LD)*b+E(LC)*a + (alpha + beta)/2.0 * (E(LD)-E(LC));
                                    //std::clog << "a=" << a << " pia=" << pia << ", v=" << v <<  ", EC=" << E(LC) << " w=" << w << " b=" << b << " pib=" << pib << ", ED=" << E(LD) << " crit=" << EV << std::endl;

                                }
       // std::cout <<std::endl;
                            }

                }
//std::clog << opt.V << " vs " << altEV << std::endl;
       assert(!opt.isnan());
       return opt;
   }


    tprice lasta = khundefprice;
    tprice lastb = klundefprice;

    tvolume lastv = 0;
    tvolume lastw = 0;

    bool finitialized;
    tvalueiterationmarketmakersetting fsetting;
    gridapproxfn V;
};


double Phi(double x)
{
  return 0.5 * (1 + erf(x / sqrt(2)));
}


int main()
{
    try
    {
        constexpr bool chronos = false;
        constexpr bool logging = true;
        constexpr tabstime runningtime = 3600;
        twallet endowment(5000,100);
        tmarketdef def;

        def.loggingfilter.fprotocol = true;
//       def.loggedstrategies.push_back(2);
        def.directlogging = true;
        def.demandupdateperiod = 0.1;


//        competitor<lessnaivemmstrategy<10,3600*5>,chronos> ln("mensinaivka");
        competitor<naivemmstrategy<10,1800>,chronos,true> n1("naivkapomala");
        competitor<naivemmstrategy<10,3600>,chronos,true> n2("naivka");
        competitor<naivemmstrategy<10,7200>,chronos,true> n3("naivka");

        competitor<valueiterationmarketmaker,chronos> ts("tested");
        competitor<initialstrategy<90,110>,chronos,true> is("is");
//        competitor<cancellinguniformluckockstrategy<1,200,360,3600,true>,
//                          chronos,true> cul("luckock");
        competitor<cancellingmaslovorderplacer<100,100>,chronos,true> cul("corderplacer");

        std::ofstream det("details.txt");
        std::ostream& os=std::clog;
        int seed = 234;
        unsigned n=500;
        double origalpha = 0.05;
        os << "t,lambda,alpha,mean,stderr,cvar"<<origalpha << std::endl;
        for(int lambda=-1; lambda<=1; lambda++)
            for(tabstime t=100 * sqrt(10); t >= 0.9999; t /=  sqrt(sqrt(10)) )
            {
                gs.interval = t;
                gs.lambda = fabs(lambda);
                std::ostringstream s;
                double alpha;
                switch(lambda)
                {
                case -1:
                    alpha = 1-Phi(-sqrt(-log(2*3.141592*t/3600.0*(1.0-origalpha))));
                    break;
                case 0 :
                    alpha = 0;
                    break;
                case 1 :
                    alpha = origalpha;
                }
                gs.alpha = alpha;
                s << t << "," << gs.lambda << "," << alpha;
                os << s.str();
                det << s.str();

                statcounter sc;
                finitedistribution<double> fd;
                std::vector<double> hist(30);
                double histdelta = 1000.0;
                for(unsigned j=0; j<n; j++)
                {
                    auto r = test<chronos,true,logging,true,fairpriceds<3*3600,10>>(
                                              {&is,&cul,&ts ,&n1,&n2,&n3},runningtime,
                                              {twallet::infinitewallet(),
                                               twallet::infinitewallet(),
                                               endowment,endowment,endowment,endowment},
                                               def, seed++, det);
                    auto c = r->fstrategyinfos[2].totalconsumption();
                    sc.add(c);
                    fd.add(1.0/n,c);
                    hist[std::min(static_cast<unsigned>(c / histdelta),
                                  static_cast<unsigned>(hist.size()-1))]++;
                    //os << "," << c;
                }
                os << "," << sc.average() << "," << sqrt(sc.var()/sc.num)
                   << "," << fd.lowerCVaR(origalpha);
                for(unsigned i=0; i<hist.size(); i++)
                    os << "," << hist[i];
                os << std::endl;
            }
    }
    catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    catch (const char* m)
    {
           std::cerr << m << std::endl;
           return 1;
    }
    catch (const std::string& m)
    {
           std::cerr << m << std::endl;
           return 1;
    }
    catch(...)
    {
        std::cerr << "unknown exception" << std::endl;
        return 1;
    }

    return 0;
}



