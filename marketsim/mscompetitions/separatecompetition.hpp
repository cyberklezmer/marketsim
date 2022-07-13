#ifndef SEPARATECOMPETITION_HPP
#define SEPARATECOMPETITION_HPP

#include "mscompetitions/zicompetition.hpp"
#include "msstrategies/maslovstrategy.hpp"
#include "msstrategies/luckockstrategy.hpp"

namespace marketsim
{


template <bool logging = false>
inline void separatecompetition(std::vector<competitorbase<false>*> acompetitors,
                          std::vector<competitorbase<false>*> azistrategies,
                          const tcompetitiondef& adef,
                          twallet endowment)
{
    std::vector<statcounter> res;
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
            arg.push_back(acompetitors[c]);
            std::vector<competitionresult> r
                    = zicompetition<false,logging>(
                      arg,azistrategies[z],endowment,def, std::clog);
            res.push_back(r[0].consumption);
            resv.push_back(r[0].value);
            def.seed++;
        }
    std::ostream& o = std::cout;

    o << "Results of separate competition" << std::endl;
    unsigned i=0;
    unsigned j=0;
    o << "zi/competitor";
    for(unsigned c=0; c<acompetitors.size(); c++)
        o << ",c-" << acompetitors[c]->name();
    for(unsigned c=0; c<acompetitors.size(); c++)
        o << ",v-" << acompetitors[c]->name();
    o << std::endl;
    for(unsigned z=0; z<azistrategies.size(); z++)
    {
        o << azistrategies[z]->name();
        for(unsigned c=0; c<acompetitors.size(); c++)
        {
            statcounter sc = res[i++];
            o << "," << sc.average() << "(" << sqrt(sc.averagevar()) << ")";
        }
        for(unsigned c=0; c<acompetitors.size(); c++)
        {
            statcounter sc = resv[j++];
            o << "," << sc.average() << "(" << sqrt(sc.averagevar()) << ")";
        }
        o << std::endl;
    }
}

template <bool logging>
inline void separatezicomp(std::vector<competitorbase<false>*> acompetitors)
{
    twallet endowment(5000,100);
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

    competitor<cancellingmaslovstrategy<10,900,360,30>> mss("maslovslow");
    competitor<cancellinguniformluckockstrategy<10,200,360,90,false>> lss("luckockslow");

    separatecompetition<logging>(acompetitors,{&msf,&lsf,&mss,&lss},cdef,twallet(5000,100));
}



}

#endif // SEPARATECOMPETITION_HPP
