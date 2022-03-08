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

unsigned iwashere = 0;

class foostrategy: public tstrategy
{
public:
       foostrategy() : tstrategy("foo") {}

       virtual void trade(twallet) override
       {
           while(!endoftrading())
           {

               trequest::teraserequest er(false);

               tpreorderprofile pp;
               tpreorder o(iwashere+2,1);
               pp.A.add(o);

               tpreorder p(iwashere+1,1);
               pp.B.add(p);

               request({pp,er,0});
               iwashere++;
               sleepfor(1);
           }
       }
};

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
protected:

public:
       luckockstrategy(tprice maxprice, double meantime )
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

double getduration(unsigned nstrategies, tmarketdef def = tmarketdef())
{
    int d = 100;
    tmarketdef df =def;
    df.chronosduration = chronos::app_duration(d);
    tmarket m(10000*df.chronos2abstime,df);
    vector<twallet> e(nstrategies,
                      twallet(numeric_limits<tprice>::max()/2, numeric_limits<tvolume>::max()/2));
    vector<tstrategy*> s(nstrategies,new calibratingstrategy());
    m.run(s,e);
    return m.results()->fremainingdurations.average();
}


int main()
{
    try
    {
        int d = getduration(1);
        cout << "Calibrate " << d << endl;
        return 0;
        tmarket m(1000);

        ostringstream l;

        m.setlogging(l);

        twallet e(5000,100);

        luckockstrategy s(100,1);

        std::cout << "start" << std::endl;

        m.run({&s},{e});
        std::cout << "stop" << std::endl;

        m.results()->fhistory.output(l,1);
        std::cout << l.str();

        cout << "I was there " << iwashere << " times." << endl;

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



