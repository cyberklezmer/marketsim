#ifndef CALIBRATE_HPP
#define CALIBRATE_HPP

#include "marketsim.hpp"

namespace marketsim
{

class calibrateerror: public error
{
public:
   calibrateerror(const std::string &what = "") : error(what) {}
};


class calibratingstrategy : public tstrategy
{
public:
    calibratingstrategy() : tstrategy() {}
    virtual void trade(twallet) override
    {
        while(!endoftrading())
        {
            bool buy = uniform() > 0.5;
            tprice lprice = 1+uniform()*100;

            tpreorderprofile pp;
            if(buy)
            {
                tpreorder p(lprice,1);
                pp.B.add(p);
            }
            else
            {
                tpreorder o(lprice,1);
                pp.A.add(o);
            }
            request({pp,trequest::teraserequest(true),0});
        }
    }
};

template <bool logging>
inline int findduration(unsigned nstrategies, tmarketdef def = tmarketdef())
{
    std::vector<tstrategy*> garbage;
    std::clog << "Finding duration for " << nstrategies << " agents on this PC." << std::endl;
    int d = 1000;
    for(unsigned i=0; i<10; i++,d*= sqrt(10))
    {
        tmarketdef df =def;
        df.chronosduration = chronos::app_duration(d);
        tmarket m(1000*df.ticktime(),df);
        std::ofstream o("finddurationtestlog.log");
        if constexpr(logging)
        {
            if(!o)
                throw calibrateerror("Cannot create temporary log");
            m.setlogging(o);
        }

        // tbd create infinitywallet
        std::vector<twallet> e(nstrategies,twallet::infinitewallet());
        competitor<calibratingstrategy> c;
        std::vector<competitorbase<true>*> s;
        for(unsigned j=0; j<nstrategies; j++)
            s.push_back(&c);
        try {
            std::clog << "d = " << d << std::endl;
            m.run(s,e,garbage);

            if(garbage.size()) /// tbd speciální výjimka
                throw marketsimerror("Internal error: calibrating strategy unterminated.");
            double rem = m.results()->frunstat.fextraduration.average();
            std::clog << "remaining = " << rem << std::endl;
            if(rem > 0)
                break;
        }
        catch (calibrateerror)
        {
            throw;
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
    d *= sqrt(10);
    std::clog << d << " found suitable " << std::endl;
    return d;
}


template <bool logging = false>
inline void calibrate(tmarketdef& def, unsigned ncompetitors)
{
    std::clog << "Calibrating for " << ncompetitors << " competitors." << std::endl;
    def.chronosduration = chronos::app_duration(findduration<logging>(ncompetitors,def));
    std::clog << "ticktime() = " << def.ticktime() << std::endl;
}


} // namespace
#endif // CALIBRATE_HPP
