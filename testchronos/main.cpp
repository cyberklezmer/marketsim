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

class selfishstrategy : public tstrategy
{
public:
    selfishstrategy() : tstrategy() {}
    virtual void trade(twallet) override
    {
        for(;;)
            ;
    }
};



class calibratingstrategy : public tstrategy
{
public:
    calibratingstrategy() : tstrategy() {}
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

            request({pp,trequest::teraserequest(true),0});

        }
    }
};

class luckockstrategy: public teventdrivenstrategy
{
       tprice fmaxprice;
public:
       luckockstrategy(tprice maxprice=100, double meantime=1)
           : teventdrivenstrategy(meantime,true),
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


class naivemmstrategy: public teventdrivenstrategy
{
public:
       naivemmstrategy(double interval=1)
           : teventdrivenstrategy(interval, false)
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


template <bool logging>
int findduration(unsigned nstrategies, tmarketdef def = tmarketdef())
{
    std::vector<tstrategy*> garbage;
    cout << "Finding duration for " << nstrategies << " agents on this PC." << endl;
    int d = 10;
    for(unsigned i=0; i<10; i++,d*= 10)
    {
        tmarketdef df =def;
        df.chronosduration = chronos::app_duration(d);
        tmarket m(1000*df.chronos2abstime,df);
        std::ofstream o("xxxlogxxx.log");
        if constexpr(logging)
        {
            if(!o)
                throw marketsimerror("Cannot create temporary log");
            m.setlogging(o);
        }

        vector<twallet> e(nstrategies,
                          twallet(numeric_limits<tprice>::max()/2, numeric_limits<tvolume>::max()/2));
        competitor<calibratingstrategy> c;
        std::vector<competitorbase<true>*> s;
        for(unsigned j=0; j<nstrategies; j++)
            s.push_back(&c);
        try {
            cout << "d = " << d << endl;
            m.run(s,e,garbage);
            if(garbage.size())
                throw marketsimerror("Internal error: calibrating strategy unterminated.");
            double rem = m.results()->frunstat.fextraduration.average();
            cout << "remaining = " << rem << endl;
            if(rem > 0)
                break;
        }
        catch (std::runtime_error& e)
        {
            std::cout << "Error:" << e.what() << std::endl;
        }
        catch (...)
        {
            std::cout << "Unknown error" << std::endl;
        }
    }
    cout << d << " found suitable " << endl;
    return d;
}


template <bool chronos, bool directlog>
void test()
{
    tmarket m(10);

    m.setlogging(cout);

    m.setdirectlogging(directlog);

    twallet e(5000,100);

    std::cout << "start " << std::endl;

    competitor<luckockstrategy,chronos> cl;
    competitor<naivemmstrategy,chronos> cn;
    competitor<selfishstrategy,chronos> cs;
    std::vector<tstrategy*> garbage;
    try
    {
        if(!m.run<chronos>({&cl,&cl,&cn,&cs},{e,e,e,e},garbage))
            cout << "Selfish stretegy!" << endl;

        auto r = m.results();
        for(unsigned i=0; i<r->n(); i++)
        {
            cout << i << " " << r->fstrategyinfos[i].name() << ": ";
            r->fstrategyinfos[i].wallet().output(cout);
            if(r->fstrategyinfos[i].endedbyexception())
                cout << " (ended by exception: " << r->fstrategyinfos[i].errmsg() << ")";
            cout << endl;
        }
        std::cout << r->frunstat.fextraduration.average() << " out of "
             << m.def().chronosduration.count() << " processor ticks unexploited."
             << std::endl;
        std::cout << "success" << std::endl;

    }
    catch (std::runtime_error& e)
    {
        std::cout << "Error:" << e.what() << std::endl;
    }
    catch (...)
    {
        std::cout << "Unknown error" << std::endl;
    }
}

template <bool chronos>
void comp()
{

    twallet e(5000,100);

    competitor<selfishstrategy,chronos> cs;
    competitor<luckockstrategy,chronos> cl;
    competitor<naivemmstrategy,chronos> cn;
    std::vector<competitorbase<chronos>*> competitors = {&cl,&cl,&cl,&cn,&cn,&cs};


    tmarketdef def;
    if(chronos)
    {
        def.chronosduration = chronos::app_duration(findduration<false>(competitors.size(),def));
    }

    tcompetition comp;

    std::vector<tstrategy*> garbage;
    auto res = comp.run<chronos,true>(competitors,e,1000,100,garbage,cout,def);
    cout << "Results:" << endl;
    for(unsigned i=0;i<res.size();i++)
    {
        cout << res[i].average() << " (" << sqrt(res[i].var()) << ")" << endl;
    }
}


int main()
{
    try
    {
//        int d = findduration(100);
//        cout << "Calibrate gave result " << d << endl;
//        return 0;
//        test<true,false>(); // with chronos
//        test<false,false>(); // without chronos
        comp<true>();
//        comp<false>();
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



