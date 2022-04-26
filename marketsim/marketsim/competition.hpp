#ifndef COMPETITION_HPP
#define COMPETITION_HPP

#include "marketsim.hpp"

namespace marketsim
{


/// competition parameters
struct tcompetitiondef
{
    /// endowment each competing (not built-in) strategy gets
    twallet endowment;
    /// number of runs within the competition
    unsigned samplesize;
    /// duration of a single run
    tabstime timeofrun;
    /// warmup time (competing strategies are activavated as late as at this time)
    tabstime warmup = 1;
    /// market parameters
    tmarketdef marketdef = tmarketdef();
};

/// result of a single strategy within the competition
struct competitionresult
{
    /// total (raw) consumption achieved
    statcounter consumption;
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
template <bool chronos=true, bool calibrate=true, bool logging = false>
inline std::vector<competitionresult>
        compete(std::vector<competitorbase<chronos>*> competitors,
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
       o << competitors[j]->name() << j << ",";
    o << std::endl;

    rescsv << "turn,id,c,m,s,lastp" << std::endl;

    unsigned nobs = 0;
    for(unsigned i=0; nobs<compdef.samplesize && i<compdef.samplesize * 2; i++)
    {
        o << i << ",";

        tmarket m(compdef.timeofrun,compdef.marketdef);

        std::ostringstream os;
        os << "log" << i << ".csv";
        std::ofstream log(os.str());
        if(logging)
        {
            m.setlogging(log,m.def().loggingfilter);
            if(def.directlogging)
               m.setdirectlogging(true);
        }
        std::vector<twallet> es;
        for(unsigned j=0; j<n;j++)
            es.push_back(competitors[j]->isbuiltin()
                         ? twallet::infinitewallet()
                         : compdef.endowment);
        try
        {
            auto res = m.run<chronos>(competitors,es,garbage,33+i*22);
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
                double p = r->fhistory.p(std::numeric_limits<double>::max());
                for(unsigned j=0; j<n; j++)
                {
                    auto& tr= r->fstrategyinfos[j];
//                    auto& e = es[j];
//                    double m = (tr.wallet().money() - e.money());
                    double c = tr.totalconsumption();

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

} // namespace

#endif // COMPETITION_HPP
