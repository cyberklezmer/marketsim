#include <vector>
#include <iostream>
#include <limits>
#include <fstream>
#include <algorithm>
#include <random>

#include "marketsim.hpp"
#include "msstrategies/stupidtraders.hpp"
#include "msstrategies/marketorderplacer.hpp"
#include "marketsim/competition.hpp"
#include "marketsim/tests.hpp"
#include "msstrategies/adpmarketmaker.hpp"
#include "msstrategies/naivemmstrategy.hpp"
#include "msstrategies/lessnaivemmstrategy.hpp"
#include "msstrategies/firstsecondstrategy.hpp"
#include "msstrategies/liquiditytakers.hpp"
#include "msstrategies/parametricmm.hpp"
#include "msstrategies/maslovorderplacer.hpp"
#include "msstrategies/initialstrategy.hpp"
#include "msstrategies/marketorderplacer.hpp"
#include "msstrategies/heuristicstrategy.hpp"
#include "msstrategies/buyer.hpp"
#include "msdss/rawds.hpp"
#include "mscompetitions/luckockcompetition.hpp"
#include "mscompetitions/dsmaslovcompetition.hpp"
#include "mscompetitions/zicompetition.hpp"
#include "mscompetitions/separatecompetition.hpp"
#include "mscompetitions/bscompetitions.hpp"

using namespace marketsim;

double Phi(double x)
{
  return 0.5 * (1 + erf(x / sqrt(2)));
}

class xxx : public teventdrivenstrategy
{
public:
    xxx(tprice b=90, tprice a=100, tvolume v=1)
        : teventdrivenstrategy(0), fb(b), fa(a), fv(v) {}
    virtual trequest event(const tmarketinfo& info, tabstime t, trequestresult*)
    {
        if(t<5)
        {
            const twallet& w = info.mywallet();
            assert(w.stocks() >= fv);
            assert(w.money() >= fv * fb);

            tpreorderprofile pp;

            tpreorder p(fb,fv);
            pp.B.add(p);
            tpreorder o(fa,fv);
            pp.A.add(o);
            setinterval(1);
            return trequest({pp,trequest::teraserequest(false),0});
        }
        else
        {
            trequest r;
            r.addsellmarket(10);
            return r;
        }

    }
private:
    tprice fb;
    tprice fa;
    tvolume fv;
};


int main()
{
    try
    {
            constexpr bool logging = true;

            competitor<stupidtrader<1, 3600, true>> ub("unitbuyer");
            competitor<stupidtrader<std::numeric_limits<tvolume>::max(), 3600, true>> ab("allbuyer");
            competitor<buyer> ts("buyer");

            separatebszicomp<true, logging>({&ts,&ub,&ab});

/*        constexpr bool chronos = false;
        constexpr bool logging = true;
        constexpr tabstime runningtime = 3600;
        twallet endowment(5000,100);
        tmarketdef def;

        def.loggingfilter.fprotocol = true;
        def.loggingfilter.fsettle = true;
//       def.loggedstrategies.push_back(2);
        def.directlogging = true;
        def.demandupdateperiod = 0.1;


        competitor<naivemmstrategy<10,1800>,chronos,true> n1("naivkapomala");
        competitor<naivemmstrategy<10,3600>,chronos,true> n2("naivka");
        competitor<naivemmstrategy<10,7200>,chronos,true> n3("naivka");

        competitor<heuristicstrategy<false>,chronos> ts("tested");
        competitor<xxx,chronos,true> is("is");
//        competitor<cancellinguniformluckockstrategy<1,200,360,3600,true>,
//                          chronos,true> cul("luckock");
        competitor<cancellingmaslovorderplacer<100,100>,chronos,true> cul("corderplacer");

        tcompetitiondef cdef;
        cdef.timeofrun = 3600;
        cdef.marketdef = def;
        //cdef.marketdef.loggingfilter.fprotocol = true;
        //luckockcompetition<false>({&ts,&n1,&n2,&n3}, endowment, cdef,std::clog );
        dsmaslovcompetition<false,true>({&is,&ts,&n1,&n2,&n3}, endowment, cdef,std::clog );
        int seed = 12121;
        std::ostream& os=std::clog;
        for(unsigned i=1; i<10; i++)
        {
            statcounter sc;
            finitedistribution<double> fd;
            std::vector<double> hist(30);
            double histdelta = 1000.0;
            std::ofstream dets("dets.csv");
            unsigned n = 10;
            for(unsigned j=0; j<n; j++)
            {
  //              gs.desiredmean = t;
                auto r = test<chronos,true,logging,true,fairpriceds<3600,10>>(
                                          {&is,&cul,&ts ,&n1,&n2,&n3},runningtime,
                                          {twallet::infinitewallet(),
                                           twallet::infinitewallet(),
                                           endowment,endowment,endowment,endowment},
                                           def, seed++, dets);
                auto c = r->fstrategyinfos[2].totalconsumption();
                sc.add(c);
                fd.add(1.0/n,c);
                hist[std::min(static_cast<unsigned>(c / histdelta),
                              static_cast<unsigned>(hist.size()-1))]++;
                //os << "," << c;

            }
            os << theuristicstrategysetting().desiredmean <<"," << sc.average() << "," << sqrt(sc.var()/sc.num)
               << "," << fd.lowerCVaR(0.05);
            for(unsigned i=0; i<hist.size(); i++)
                os << "," << hist[i];
            os << std::endl;
        } */
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



