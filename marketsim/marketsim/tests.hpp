#ifndef MARKETSIMTESTS_HPP
#define MARKETSIMTESTS_HPP

#include "marketsim.hpp"
#include "marketsim/competition.hpp"
#include "msstrategies/maslovstrategy.hpp"

namespace marketsim
{

template <bool chronos=true, bool calibrate = true, bool logging = false>
inline void competitionwithmaslov(std::vector<competitorbase<chronos>*> acompetitors,
                                  tabstime timeofrun,
                                  twallet endowment,
                                  const tmarketdef& adef,
                                  std::ostream& protocol)
{
   std::vector<tstrategy*> garbage;

   std::vector<competitorbase<chronos>*> competitors = acompetitors;

   competitor<maslovstrategy,chronos,true> cm("maslov(internal)");

   competitors.push_back(&cm);

   std::ofstream rescsv("competitionwithmalsov.csv");
   if(!rescsv)
       throw std::runtime_error("Cannot open competitionwithmalsov.csv");

   tcompetitiondef cd;
   cd.endowment = endowment;
   cd.samplesize = 100;
   cd.timeofrun = timeofrun;
   cd.marketdef = adef;

   auto res = compete<chronos,calibrate,logging>(competitors,cd,rescsv,garbage,std::clog);

   protocol << "Protocol of competition." << std::endl;
   for(unsigned i=0;i<res.size();i++)
   {
      const statcounter& c = res[i].consumption;
      protocol << competitors[i]->name() << " " << c.average()
         << " (" << sqrt(c.var() / c.num ) << ")"
         << " [" << res[i].nruns << " runs, " << res[i].nexcepts << " exceptions, "
         << res[i].noverruns << " overruns]"
         << std::endl;
   }
   if(garbage.size())
        std::clog << garbage.size()
            << " overrun strategies in garbage, sorry for memory leaks." << std::endl;
}



template <bool chronos, bool calibrate, bool logging>
inline void test(std::vector<competitorbase<chronos>*> competitors,
                 tabstime runningtime,
                 const twallet& endowment,
                 const tmarketdef& adef)
{
    auto n = competitors.size();
    tmarketdef def = adef;
    if constexpr(chronos && calibrate)
        def.calibrate<logging>(n,std::clog);

    tmarket m(runningtime,adef);

    if(logging)
        m.setlogging(std::cout,m.def().loggingfilter);

    std::vector<twallet> es(n, endowment);

    std::clog << "running " << n << " strategies" << std::endl;

    std::vector<tstrategy*> garbage;
    try
    {
        auto res = m.run<chronos>(competitors,es,garbage);
        for(unsigned i=0; i<res.size(); i++)
            if(res[i])
                std::cout << "Strategy with index " << i << " did not release control." << std::endl;

        auto r = m.results();
        std::cout << "Results" << std::endl;
        for(unsigned i=0; i<n; i++)
        {
            std::cout << i << " " << r->fstrategyinfos[i].name() << " wallet: ";
            r->fstrategyinfos[i].wallet().output(std::cout);
            std::cout << " consumption: " << r->fstrategyinfos[i].totalconsumption();
            if(r->fstrategyinfos[i].isendedbyexception())
                std::cout << " (ended by exception: " << r->fstrategyinfos[i].errmsg() << ")";
            std::cout << std::endl;
        }
        if(chronos)
            std::cout << r->fextraduration.average() << " out of "
                 << m.def().chronosduration.count() << " processor ticks unexploited."
                 << std::endl;
        std::cout << "success" << std::endl;
    }
    catch (std::runtime_error& e)
    {
        std::cerr << "Error:" << e.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "Unknown error" << std::endl;
    }

}




} // namespace

#endif // MARKETSIMTESTS_HPP
