#ifndef MARKETSIMTESTS_HPP
#define MARKETSIMTESTS_HPP

#include "marketsim.hpp"
#include "competition.hpp"

namespace marketsim
{

template <bool chronos, bool logging>
inline void test(std::vector<competitorbase<chronos>*> competitors, tabstime runningtime = 100)
{
    tmarket m(runningtime);

    if(logging)
        m.setlogging(std::cout);

    auto n = competitors.size();

    std::vector<twallet> es(n, twallet(5000,100));

    std::cout << "start in to run" << std::endl;

    std::vector<tstrategy*> garbage;
    try
    {
        if(!m.run<chronos>(competitors,es,garbage))
            std::cout << "Selfish stretegy!" << std::endl;

        auto r = m.results();
        std::cout << "Results" << std::endl;
        for(unsigned i=0; i<n; i++)
        {
            std::cout << i << " " << r->fstrategyinfos[i].name() << " wallet: ";
            r->fstrategyinfos[i].wallet().output(std::cout);
            std::cout << " consumption: " << r->fstrategyinfos[i].totalconsumption();
            if(r->fstrategyinfos[i].endedbyexception())
                std::cout << " (ended by exception: " << r->fstrategyinfos[i].errmsg() << ")";
            std::cout << std::endl;
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

template <typename S, bool chronos, bool logging>
inline void testcompete(tabstime runningtime = 100)
{
    competitor<S,chronos> s;
    competitor<maslovstrategy,chronos> m;
    test<chronos,logging>({&s,&m},runningtime);
}

template <typename S, bool chronos, bool logging>
inline void testsingle(tabstime runningtime = 100)
{
    competitor<S,chronos> s;
    test<chronos,logging>({&s},runningtime);
}



template <typename S, bool chronos>
void competesingle()
{
    twallet e(5000,100);

    competitor<S,chronos> cs;

    competition<chronos>({&cs}, std::clog);
}



} // namespace

#endif // MARKETSIMTESTS_HPP
