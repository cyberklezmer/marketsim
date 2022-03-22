#ifndef COMPETITION_HPP
#define COMPETITION_HPP

#include "calibrate.hpp"
#include "maslovstrategy.hpp"

namespace marketsim
{

/// Evaluates the current strategies by running \p nruns simulation of length \p timeofrun.
/// For each strtategy it returns mean value of its profit (comparison to the "hold" strategy,
/// i.e. doing nothing) and the standard deviation.

struct competitionresult
{
   statcounter consumption;
   unsigned nruns = 0;
   unsigned nexcepts = 0;
   unsigned noverruns = 0;
};

template <bool chronos=true, bool logging = false>
inline std::vector<competitionresult>
        compete(std::vector<competitorbase<chronos>*> competitors,
                    twallet endowment, unsigned desiredobs,
                    tabstime timeofrun,
                    std::ostream& rescsv,
                    std::vector<tstrategy*> &garbage,
                    std::ostream& o = std::cout,
                    tmarketdef adef = tmarketdef())
{
    auto n = competitors.size();
    tmarketdef def = adef;
    if(chronos)
        def.chronosduration = chronos::app_duration(findduration<logging>(n,def));

    std::vector<competitionresult> ress(n);

    o << "turn, runresult, valid,result,tickperc";
    for(unsigned j=0; j<n; j++)
       o << competitors[j]->name() << j << ",";
    o << std::endl;

    rescsv << "turn,id,c,m,s,lastp" << std::endl;

    unsigned nobs = 0;
    for(unsigned i=0; nobs<desiredobs && i<desiredobs * 2; i++)
    {
        o << i << ",";

        tmarket m(timeofrun,def);

        std::ostringstream os;
        os << "log" << i << ".csv";
        std::ofstream log(os.str());
        if(logging)
        {
            m.setlogging(log);
            if(def.directlogging)
               m.setdirectlogging(true);
        }
        std::vector<twallet> es;
        for(unsigned j=0; j<n;j++)
            es.push_back(competitors[j]->isbuiltin() ? twallet::infinitewallet() : endowment);
        try
        {
            if(m.run<chronos>(competitors,es,garbage,33+i*22))
                o << "1,";
            else
                o << "0,";

            auto rest = static_cast<double>(m.results()->frunstat.fextraduration.average())
                          / m.def().chronosduration.count();
            if(rest < 0)
                o << "0,overflow," << rest*100 << "%,";
            else
            {
                o << "1,OK," << rest*100 << "%,";
                auto r = m.results();
                double p = r->fhistory.p(std::numeric_limits<double>::max());
                for(unsigned j=0; j<n; j++)
                {
                    auto& tr= r->fstrategyinfos[j];
//                    auto& e = es[j];
//                    double m = (tr.wallet().money() - e.money());
                    double c = 0;
                    for(unsigned k=0; k<tr.consumption().size(); k++)
                        c += tr.consumption()[k].famount;
//                    double v = c + m;
//                    if(!isnan(p))
//                          v += p * (tr.wallet().stocks() - e.stocks());
                    o << c << ",";
                    if(!competitors[j]->isbuiltin())
                    {
                        rescsv << i << "," << competitors[j]->name() << j << ","
                               << c << "," << tr.wallet().money() << ","
                               << tr.wallet().stocks() << ",";
                        if(!isnan(p))
                            rescsv << p;
                        rescsv << std::endl;
                    }
                    ress[j].consumption.add(c);
                    ress[j].nruns++;
                    if(tr.endedbyexception())
                        ress[j].nexcepts++;
                    if(tr.overrun())
                        ress[j].noverruns++;
                }
                nobs++;
            }
        }
        catch (std::runtime_error& e)
        {
            o << "0,\"error:" << e.what() << "\"," << std::endl;
        }
        catch (...)
        {
            o << "0,\"Unknown error\"" << std::endl;
        }
        o << std::endl;
    }
    return ress;
}


template <bool chronos=true, bool logging = false>
inline void competition(std::vector<competitorbase<chronos>*> acompetitors,
                        std::ostream& protocol)
{
   std::vector<tstrategy*> garbage;

   twallet endowment(5000,100);

   unsigned nruns = 100;

   std::vector<competitorbase<chronos>*> competitors = acompetitors;

   competitor<maslovstrategy,chronos,true> cm("maslov(internal)");

   competitors.push_back(&cm);

   std::ofstream rescsv("res.csv");
   if(!rescsv)
       throw std::runtime_error("Cannot open res.csv");

   auto res = compete<chronos,false>(competitors,endowment,nruns,1000,rescsv,garbage,std::clog);

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

} // namespace

#endif // COMPETITION_HPP
