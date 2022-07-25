#ifndef BSCOMPETITIONS_HPP
#define BSCOMPETITIONS_HPP

#include "mscompetitions/zicompetition.hpp"
#include "msstrategies/stupidtraders.hpp"
#include "msstrategies/maslovstrategy.hpp"
#include "msstrategies/luckockstrategy.hpp"
#include "msdss/rawds.hpp"
#include "msdss/fairpriceds.hpp"

namespace marketsim
{

template <bool buyers, typename DS, bool logging = false,
          int counterpartyeph = 3600, int counterpartyvol = 1>
inline void separatebscompetition(std::vector<competitorbase<false>*> acompetitors,
                          std::vector<competitorbase<false>*> azistrategies,
                          const tcompetitiondef& adef)
{
    std::vector<statcounter> ress;
    std::vector<statcounter> resm;
    std::vector<statcounter> resv;
    tcompetitiondef def = adef;
    for(unsigned z=0; z<azistrategies.size(); z++)
        for(unsigned c=0; c<acompetitors.size(); c++)
        {
            std::ostringstream s;
            s << azistrategies[z]->name() << "_"
              << acompetitors[c]->name() << "_";
            def.id = s.str();

            std::vector<competitorbase<false>*> arg;

            competitor<stupidtrader<counterpartyvol,counterpartyeph, false /*bulyer*/>,false>
                   ctseller("seller");
            competitor<stupidtrader<counterpartyvol,counterpartyeph, true /*bulyer*/>,false>
                   ctbuyer("buyer");
            arg.push_back(acompetitors[c]);
            if(buyers)
                arg.push_back(&ctseller);
            else
                arg.push_back(&ctbuyer);
            std::vector<competitionresult> r
                    = zicompetition<false,logging,DS>(
                      arg,azistrategies[z],twallet::emptywallet(),def, std::clog);
            resv.push_back(r[0].value);
            ress.push_back(r[0].stocks);
            resm.push_back(r[0].money);
            def.seed++;
        }
    std::ostream& o = std::cout;

    o << "Results of separate buyer/seller competition" << std::endl;
    unsigned i=0;
    unsigned j=0;
    unsigned k=0;
    o << "zi/competitor";
    for(unsigned c=0; c<acompetitors.size(); c++)
        o << ",s-" << acompetitors[c]->name();
    for(unsigned c=0; c<acompetitors.size(); c++)
        o << ",m-" << acompetitors[c]->name();
    for(unsigned c=0; c<acompetitors.size(); c++)
        o << ",v-" << acompetitors[c]->name();
    o << std::endl;
    for(unsigned z=0; z<azistrategies.size(); z++)
    {
        o << azistrategies[z]->name();
        for(unsigned c=0; c<acompetitors.size(); c++)
        {
            statcounter sc = ress[i++];
            o << "," << sc.average() << "(" << sqrt(sc.averagevar()) << ")";
        }
        for(unsigned c=0; c<acompetitors.size(); c++)
        {
            statcounter sc = resm[j++];
            o << "," << sc.average() << "(" << sqrt(sc.averagevar()) << ")";
        }
        for(unsigned c=0; c<acompetitors.size(); c++)
        {
            statcounter sc = resv[k++];
            o << "," << sc.average() << "(" << sqrt(sc.averagevar()) << ")";
        }
        o << std::endl;
    }
}

template <bool buyer, bool logging, typename DS=rawds<36,100>>
inline void separatebszicomp(std::vector<competitorbase<false>*> acompetitors)
{
    tmarketdef def;

    def.loggingfilter.fprotocol = true;
//        def.loggingfilter.fsettle = true;
//        def.loggedstrategies.push_back(2);
    def.directlogging = true;
//       def.demandupdateperiod = 0.1;
    tcompetitiondef cdef;
    cdef.timeofrun = 8*3600;
    cdef.samplesize = 10;
    cdef.marketdef = def;

    competitor<cancellingmaslovstrategy<10,3600,360,30>> msf("maslovfast");
    competitor<cancellinguniformluckockstrategy<10,200,360,3600,false>> lsf("luckockfast");

    competitor<cancellingmaslovstrategy<40,900,360,30>> mss("maslovslow");
    competitor<cancellinguniformluckockstrategy<180,200,360,90,false>> lss("luckockslow");

    separatebscompetition<buyer,DS,logging>(acompetitors,
             {&msf,&lsf,&mss,&lss},cdef);
}


} // namespace

#endif // BSCOMPETITIONS_HPP
