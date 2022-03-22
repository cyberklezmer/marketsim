#include <vector>
#include <iostream>
#include <limits>
#include <fstream>
#include <algorithm>
#include <random>

//#include "strategies.hpp"
#include "marketsim.hpp"
#include "competition.hpp"

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
               double p = (alpha + beta)/2;
               tprice c = 0;
               if(info.mywallet().money() > 5*p)
                    c = info.mywallet().money()-5*p;
               tprice mya = info.myorderprofile.a();
               tprice myb = info.myorderprofile.b();

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

                  return {pp,trequest::teraserequest(true),c};
               }
           }
           return trequest();
       }
};


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
    competitor<maslovstrategy,chronos,true> cm;
    std::vector<tstrategy*> garbage;
    try
    {
        if(!m.run<chronos>({&cn,&cm},{e,e},garbage))
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

    competitor<selfishstrategy,chronos> cs("selfish");
    competitor<luckockstrategy,chronos> cl("luckock");
    competitor<naivemmstrategy,chronos> cn("naive");
    std::vector<competitorbase<chronos>*> competitors = {&cn,&cl,&cn};

    competition<chronos>(competitors, std::clog);
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



