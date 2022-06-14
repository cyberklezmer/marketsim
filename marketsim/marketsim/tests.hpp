#ifndef MARKETSIMTESTS_HPP
#define MARKETSIMTESTS_HPP

#include "marketsim.hpp"
#include "marketsim/competition.hpp"

namespace marketsim
{



template <bool chronos, bool calibrate, bool logging, typename D = tnodemandsupply>
inline std::shared_ptr<tmarketdata> test(std::vector<competitorbase<chronos>*> competitors,
                 tabstime runningtime,
                 std::vector<twallet> endowments,
                 const tmarketdef& adef)
{
    auto n = competitors.size();
    tmarketdef def = adef;
    if constexpr(chronos && calibrate)
        def.calibrate<logging>(n,std::clog);

    tmarket m(runningtime,adef);

    if(logging)
        m.setlogging(std::cout,m.def().loggingfilter);

    std::clog << "running " << n << " strategies" << std::endl;

    std::vector<tstrategy*> garbage;
    try
    {
        auto res = m.run<chronos,D>(competitors,endowments,garbage);
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

template <bool chronos, bool calibrate, bool logging, typename D = tnodemandsupply>
inline std::shared_ptr<tmarketdata> test(std::vector<competitorbase<chronos>*> competitors,
                 tabstime runningtime,
                 const twallet& endowment,
                 const tmarketdef& adef)
{
    return test<chronos,calibrate,logging,D>(competitors,runningtime, std::vector<twallet>(competitors.size(),endowment),adef);
}


} // namespace

#endif // MARKETSIMTESTS_HPP
