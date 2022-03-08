#include <boost/math/distributions.hpp>
#include <vector>
#include <iostream>
#include <limits>
#include <fstream>
#include <algorithm>
#include <random>

//#include "strategies.hpp"
#include "marketsim.hpp"

using namespace marketsim;
using namespace std;

class randomizingstrategy: public tstrategy
{
protected:
    std::default_random_engine fengine;
    std::uniform_real_distribution<double> funiform;
    randomizingstrategy(const std::string& name) : tstrategy(name)
    {
        fengine.seed(0 /* time(0) */ );
    }
    double uniform()
    {
        return funiform(fengine);
    }
};

class calibratingstrategy : public randomizingstrategy
{
public:
    calibratingstrategy() : randomizingstrategy("") {}
    virtual void trade(twallet) override
    {
        while(!endoftrading())
        {
            while(!endoftrading())
            {
                bool buy = uniform() > 0.5;
                tprice lprice = 1+uniform()*100;

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

                request({pp,trequest::teraserequest(false),0});

            }
        }
    }

};

class luckockstrategy: public randomizingstrategy
{
       tprice fmaxprice;
       double fmeantime;
public:
       luckockstrategy(tprice maxprice=100, double meantime=1)
           : randomizingstrategy("Luckock"),
             fmaxprice(maxprice), fmeantime(meantime)
       {
       }

       virtual void trade(twallet) override
       {
           while(!endoftrading())
           {
               tabstime at = abstime();
               bool buy = uniform() > 0.5;
               tprice lprice = 1+uniform()*fmaxprice;

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

               request({pp,trequest::teraserequest(false),0});

               std::exponential_distribution<> nu(1.0 / fmeantime);

               auto next = nu(fengine);
               sleepuntil(at+next);
           }
       }
};


class naivemmstrategy: public randomizingstrategy
{
       double finterval;
public:
       naivemmstrategy(double interval=1)
           : randomizingstrategy("Naivemm"), finterval(interval)
       {
       }

       virtual void trade(twallet) override
       {
           while(!endoftrading())
           {
               auto info = getinfo();
               tprice alpha = info.alpha();
               tprice beta = info.beta();

               if(alpha != khundefprice && beta != klundefprice)
               {
                   tprice mya = info.myprofile.a();
                   tprice myb = info.myprofile.b();

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

                      request({pp,trequest::teraserequest(false),0});
                   }
               }
               sleepuntil(info.t+finterval);
            }
       }
};



double findduration(unsigned nstrategies, tmarketdef def = tmarketdef())
{
    cout << "Finding duration for " << nstrategies << " agents on this PC." << endl;
    int d = 1000;
    for(unsigned i=0; i<10; i++,d*= 10)
    {
        tmarketdef df =def;
        df.chronosduration = chronos::app_duration(d);
        tmarket m(10000*df.chronos2abstime,df);
        vector<twallet> e(nstrategies,
                          twallet(numeric_limits<tprice>::max()/2, numeric_limits<tvolume>::max()/2));
        competitor<calibratingstrategy> c;
        vector<competitorbase*> s;
        for(unsigned j=0; j<nstrategies; j++)
            s.push_back(&c);
        m.run(s,e);
        double rem = m.results()->fremainingdurations.average();
        cout << "d = " << d << ", rem = " << rem << endl;
        if(rem > 0)
            break;
    }
    cout << d << " found suitable " << endl;
    return d;
}


int main()
{
    try
    {
//        int d = findduration(1);
//        cout << "Calibrate " << d << endl;
//        return 0;
        tmarket m(1000);

        ostringstream l;

        m.setlogging(l);

        twallet e(5000,100);

        competitor<luckockstrategy> cl;
        competitor<naivemmstrategy> cn;

        std::cout << "start" << std::endl;

        m.run({&cl,&cn},{e,e});

        std::cout << "stop" << std::endl;

        auto r = m.results();
        for(unsigned i=0; i<r->n(); i++)
        {
            cout << i << ": ";
            r->ftradings[i].wallet().output(cout);
            cout << endl;
        }

//        m.results()->fhistory.output(l,1);
//        std::cout << l.str();

        return 0;
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



