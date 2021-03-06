#ifndef COMPETITION_HPP
#define COMPETITION_HPP

#include "marketsim.hpp"
#include <iostream>
#include <fstream>

namespace marketsim
{

/// competition parameters
struct tcompetitiondef
{
    /// number of runs within the competition
    unsigned samplesize = 100;
    /// duration of a single run
    tabstime timeofrun = 3600;
    int seed = 1;
    int seeddelta = 123;
    /// used e.g. to prefix log files
    std::string id = "";
    /// market parameters
    tmarketdef marketdef = tmarketdef();
};

/// result of a single strategy within the competition
struct competitionresult
{
    /// total (raw) consumption achieved
    statcounter consumption;
    /// total (raw) consumption achieved
    statcounter value;
    /// number of runs
    unsigned nruns = 0;
    /// number of runs ended by exception
    unsigned nexcepts = 0;
    /// number of runs in which the strategy failed to release control
    unsigned noverruns = 0;
};



/// Evaluates the current repeatedly running strategies corresponding to
/// \p competitors. The parameters of the competition oar in
/// \p compdef (note that calibration of marketsim::tmarketdef is not done within procedure),
/// results of individual runs are output to \p rescsv.
/// \p garbage is necessary to provide to store strategies which failed
/// to release control (which can happen only if \c chronos==true).
/// The running results are printed to \p o.
/// \return - vector of marketsim::competitionresult's corresponding to indiviudal strategies
/// \tparam chronos - RT (\c true) or ED (\c false)
/// \tparam logging - if \c true then logging is done according to
/// For each strtategy it returns mean value of its profit (comparison to the "hold" strategy,
/// i.e. doing nothing) and the standard deviation.
template <bool chronos=true, bool calibrate=true, bool logging = false, typename D=tnodemandsupply>
inline std::vector<competitionresult>
        compete(std::vector<competitorbase<chronos>*> competitors,
                std::vector<twallet> endowments,
                    const tcompetitiondef& compdef,
                    std::ostream& rescsv,
                    std::vector<tstrategy*> &garbage,
                    std::ostream& o = std::clog
                    )
{
    auto n = competitors.size();
    tmarketdef def = compdef.marketdef;
    if constexpr(chronos && calibrate)
            def.calibrate(n,o);

    std::vector<competitionresult> ress(n);

    o << "turn, runresult, valid,result,tickperc";
    for(unsigned j=0; j<n; j++)
       o << "," << competitors[j]->name() ;
    o << std::endl;

    int seed = compdef.seed;

    rescsv << "turn,id,c,m,s,lastp" << std::endl;

    unsigned nobs = 0;
    for(unsigned i=0; nobs<compdef.samplesize && i<compdef.samplesize * 2; i++)
    {
        o << i << ",";

        tmarket m(compdef.timeofrun,compdef.marketdef);
        m.seed(seed);
        seed += compdef.seeddelta;

        std::ofstream log;
        if(logging)
        {
            std::ostringstream os;
            os << compdef.id << "log" << i << ".csv";
            log.open(os.str());
            m.setlogging(log,m.def().loggingfilter);
        }

        try
        {
            auto res = m.run<chronos,D>(competitors,endowments,garbage);
            bool overflow = false;
            for(unsigned i=0; i<res.size(); i++)
                if(res[i])
                {
                    overflow = true;
                    break;
                }
            if(overflow)
                o << "1,";
            else
                o << "0,";

            auto rest = static_cast<double>(m.results()->fextraduration.average())
                          / m.def().chronosduration.count();
            if(rest < 0)
                o << "0,overflow," << rest*100 << "%,";
            else
            {
                o << "1,OK," << rest*100 << "%,";
                auto r = m.results();
                double p = r->lastdefinedp();
                for(unsigned j=0; j<n; j++)
                {
                    auto& tr= r->fstrategyinfos[j];
                    double m = tr.wallet().money() - endowments[j].money();
                    double n = tr.wallet().stocks() - endowments[j].stocks();
                    double c = tr.totalconsumption();

                    double v = c + m + n*p;
                    o << c << ",";
                    rescsv << i << "," << competitors[j]->name() << j << ","
                           << c << "," << tr.wallet().money() << ","
                           << tr.wallet().stocks() << ",";
                    if(!isnan(p))
                        rescsv << p;
                    rescsv << std::endl;
                    ress[j].consumption.add(c);
                    if(!isnan(v))
                        ress[j].value.add(v);
                    ress[j].nruns++;
                    if(tr.isendedbyexception())
                        ress[j].nexcepts++;
                    if(tr.isoverrun())
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

template <bool chronos=true, bool calibrate = true, bool logging = false,
          typename D = tnodemandsupply>
inline std::vector<competitionresult> competition(std::vector<competitorbase<chronos>*> competitors,
                                  std::vector<twallet> endowments,
                                  const tcompetitiondef& cd,
                                  std::ostream& protocol
                                  )
{
   std::vector<tstrategy*> garbage;

   std::ofstream rescsv("competition.csv");
   if(!rescsv)
       throw std::runtime_error("Cannot open competition.csv");

   auto res = compete<chronos,calibrate,logging,D>(competitors,endowments,
                                                 cd,rescsv,garbage,std::clog);

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
   return res;
}


} // namespace

#endif // COMPETITION_HPP
