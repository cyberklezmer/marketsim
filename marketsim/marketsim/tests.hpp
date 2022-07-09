#ifndef MARKETSIMTESTS_HPP
#define MARKETSIMTESTS_HPP

#include "marketsim.hpp"
#include "marketsim/competition.hpp"

namespace marketsim
{



template <bool chronos, bool calibrate, bool logging, bool allowlearning = false, typename D = tnodemandsupply>
inline std::shared_ptr<tmarketdata> test(std::vector<competitorbase<chronos>*> competitors,
                 tabstime runningtime,
                 std::vector<twallet> endowments,
                 const tmarketdef& adef,
                 int aseed = 0,
                 std::ostream& os = std::clog)
{
    auto n = competitors.size();
    tmarketdef def = adef;
    if constexpr(chronos && calibrate)
        def.calibrate<logging>(n,os);

    tmarket m(runningtime,adef);
    if(aseed != 0)
        m.seed(aseed);
    if(logging)
        m.setlogging(os,m.def().loggingfilter);

    os << "running " << n << " strategies" << std::endl;

    std::vector<tstrategy*> garbage;
    try
    {
        auto res = m.run<chronos,D,allowlearning>(competitors,endowments,garbage);
        for(unsigned i=0; i<res.size(); i++)
            if(res[i])
                os << "Strategy with index " << i << " did not release control." << std::endl;

        auto r = m.results();
        os << "Results" << std::endl;
        for(unsigned i=0; i<n; i++)
        {
            os << i << " " << r->fstrategyinfos[i].name() << " wallet: ";
            r->fstrategyinfos[i].wallet().output(os);
            os << ", consumption: " << r->fstrategyinfos[i].totalconsumption();
            if(r->fstrategyinfos[i].isendedbyexception())
                os << " (ended by exception: " << r->fstrategyinfos[i].errmsg() << ")";
            if(!chronos)
            {
                statcounter ec = r->fstrategyinfos[i].comptimes();
                os << " event run " << ec.num << " times, averagely for " << ec.average() << " sec";
            }
            os << std::endl;
        }
        if(chronos)
            os << r->fextraduration.average() << " out of "
                 << m.def().chronosduration.count() << " processor ticks unexploited."
                 << std::endl;
        os << "success" << std::endl;
        return r;
    }
    catch (std::runtime_error& e)
    {
        std::cerr << "Error:" << e.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "Unknown error" << std::endl;
    }
    return 0;
}

template <bool chronos, bool calibrate, bool logging, bool allowlearning = false, typename D = tnodemandsupply>
inline std::shared_ptr<tmarketdata> test(std::vector<competitorbase<chronos>*> competitors,
                 tabstime runningtime,
                 const twallet& endowment,
                 const tmarketdef& adef,
                 int aseed = 0,
                 std::ostream& os = std::clog)
{
    return test<chronos,calibrate,logging,allowlearning, D>(competitors,runningtime, std::vector<twallet>(competitors.size(),endowment),adef,aseed,os);
}


} // namespace

#endif // MARKETSIMTESTS_HPP
