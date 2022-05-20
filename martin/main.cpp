#pragma warning(disable : 4996)
#include <vector>
#include <iostream>
#include <limits>
#include <fstream>
#include <algorithm>
#include <random>

#include "marketsim.hpp"
#include "marketsim/competition.hpp"
#include "marketsim/tests.hpp"
#include "msstrategies/tadpmarketmaker.hpp"
#include "msstrategies/naivemmstrategy.hpp"
#include "msstrategies/firstsecondstrategy.hpp"
#include "msstrategies/liquiditytakers.hpp"

using namespace marketsim;


int main()
{
    try
    {
        constexpr bool chronos = false;
        constexpr bool logging = false;
        constexpr tabstime runningtime = 1000;
        twallet endowment(5000,100);
        tmarketdef def;

    competitor<firstsecondstrategy<40,10>,chronos,true> fss;

    // this strategy simulates liquidity takers:
    // 360 times per hour on average it puts a market order with volume 5 on average
    competitor<liquiditytakers<360,5>,chronos,true> lts;


    // competing strategies

    // this strategy just wants to put orders into spread and when it
    // has more money than five times the price, it consumes. The volume of
    // the orders is always 10

        competitor<naivemmstrategy<10>,chronos> nmm;


    // our ingenious strategy
        using testedstrategy = tadpmarketmaker;

        competitor<testedstrategy,chronos> ts;

        tcompetitiondef cdef;
        cdef.timeofrun = runningtime;
        cdef.endowment = endowment;
        cdef.marketdef = def;
        cdef.samplesize = 30;

        competition<chronos,true,logging>({&fss,&lts,&nmm,&ts}, cdef, std::clog);




//std::cout << std::endl <<"With chronos: " << std::endl;
//        testcompetetwo<tadpmarketmaker,naivemmstrategy,true,false>(1000);


//        testsingle<tadpmarketmaker,false,false>(); // without chronos
//        testcompete<tadpmarketmaker,false,true>(10);
//        competesingle<tadpmarketmaker,false>();
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



/*
 *

class luckockstrategy: public teventdrivenstrategy
{
       tprice fmaxprice;
public:
       luckockstrategy(tprice maxprice=100, double meantime=1)
           : teventdrivenstrategy(meantime,true),
             fmaxprice(maxprice)
       {
       }

       virtual trequest event(const tmarketinfo&, tabstime, bool)
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



class foostrategy: public teventdrivenstrategy
{
public:
       foostrategy(double interval=1)
           : teventdrivenstrategy(interval, false)
       {
       }

       virtual trequest event(const tmarketinfo&, tabstime, bool)
       {
           static int i=1;
          tpreorderprofile pp;

          pp.B.add(tpreorder(100,1));
          pp.A.add(tpreorder(101,1));
          pp.B.add(tpreorder(100-i,1));
          pp.A.add(tpreorder(101+i,1));
          i++;

          return {pp,trequest::teraserequest(true),0};
       }
};


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

template <bool chronos, bool directlog>
void test()
{
    tmarket m(3);

    m.setlogging(cout);

    m.setdirectlogging(directlog);

    twallet e(5000,100);

    std::cout << "start " << std::endl;

//    competitor<luckockstrategy,chronos> cl;
    competitor<naivemmstrategy,chronos> cn;
    competitor<foostrategy,chronos> cf;
//    competitor<selfishstrategy,chronos> cs;
    competitor<maslovstrategy,chronos,true> cm;
    competitor<tadpmarketmaker,chronos> ca;
    std::vector<tstrategy*> garbage;
    try
    {
        if(!m.run<chronos>({&ca, &cf},{e,e},garbage))
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


*/