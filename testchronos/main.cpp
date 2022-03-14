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

class calibratingstrategy : public randomizingstrategy
{
public:
    calibratingstrategy() : randomizingstrategy("") {}
    virtual void trade(twallet) override
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

};

class luckockstrategy: public asynchronousstrategy
{
       tprice fmaxprice;
public:
       luckockstrategy(tprice maxprice=100, double meantime=1)
           : asynchronousstrategy("Luckock",meantime,true),
             fmaxprice(maxprice)
       {
       }

       virtual trequest event(const tmarketinfo&)
       {
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

           return trequest({pp,trequest::teraserequest(false),0});
       }
};


class naivemmstrategy: public asynchronousstrategy
{
public:
       naivemmstrategy(double interval=1)
           : asynchronousstrategy("Naivemm", interval, false)
       {
       }

       virtual trequest event(const tmarketinfo& info)
       {
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

                  return {pp,trequest::teraserequest(true),0};
               }
           }
           return trequest();
       }
};



double findduration(unsigned nstrategies, tmarketdef def = tmarketdef())
{
    cout << "Finding duration for " << nstrategies << " agents on this PC." << endl;
    int d = 10;
    for(unsigned i=0; i<10; i++,d*= 10)
    {
        tmarketdef df =def;
        df.chronosduration = chronos::app_duration(d);
        tmarket m(10000*df.chronos2abstime,df);
        vector<twallet> e(nstrategies,
                          twallet(numeric_limits<tprice>::max()/2, numeric_limits<tvolume>::max()/2));
        competitor<calibratingstrategy> c;
        std::vector<competitorbase<tstrategy>*> s;
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
        int d = findduration(2);
        cout << "Calibrate " << d << endl;
        return 0;
        tmarketdef def;
        def.chronosduration = chronos::app_duration(100);
        tmarket m(10,def);

        ostringstream l;

        m.setlogging(l);

        twallet e(5000,100);

        std::cout << "start " << std::endl;

//        competitor<luckockstrategy,asynchronousstrategy> cl;
//        competitor<naivemmstrategy,asynchronousstrategy> cn;
//        m.run<false,asynchronousstrategy>({&cl,&cn},{e,e});

//m.run<false,asynchronousstrategy>({&cl,&cn},{e,e});

       competitor<luckockstrategy,tstrategy> cl;
        competitor<naivemmstrategy,tstrategy> cn;
        competitor<calibratingstrategy,tstrategy> cal;
        m.run({&cl,&cal,&cal},{e,e,e});

        std::cout << "stop" << std::endl;

        auto r = m.results();
        for(unsigned i=0; i<r->n(); i++)
        {
            cout << i << ": ";
            r->ftradings[i].wallet().output(cout);
            cout << endl;
        }

//        m.results()->fhistory.output(l,1);
        std::cout << l.str();

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



