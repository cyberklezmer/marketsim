#include <boost/math/distributions.hpp>
#include <vector>
#include <iostream>
#include <limits>
#include <fstream>
#include <algorithm>
#include <random>

#include "strategies.hpp"


using namespace marketsim;

class simplebuy: public tsimplestrategy
{
public:
    simplebuy(const std::string& name,tvolume delta, ttime timeinterval) :
        tsimplestrategy(name, timeinterval,false),
        fdelta(delta)
    {}
private:
    virtual tsimpleorderprofile simpleevent(const tmarketinfo& ,
                                     const ttradinghistory& )
    {
        return buymarket(fdelta);
    }
    tvolume fdelta;
};

class trendist: public tsimplestrategy
{
public:
    trendist(const std::string& name, tvolume delta, ttime timeinterval)
        : tsimplestrategy(name, timeinterval),  fdelta(delta)
    {}
private:
    virtual tsimpleorderprofile simpleevent(const tmarketinfo& mi,
                                     const ttradinghistory& )
    {
        if(mi.t > 2*timeinterval())
        {
            double p0 = mi.history.p(mi.t);
            double p1 = mi.history.p(mi.t-timeinterval());
            if(!isnan(p0) && !isnan(p1) && fabs(p0-p1) > std::numeric_limits<double>::epsilon())
                return p0 > p1 ? buymarket(fdelta) : sellmarket(fdelta);
        }
        return noorder();
    }
    tvolume fdelta;
};

/// \p evaluate: true if the strategies are to be run several times

void example(bool evaluate)
{
    // orpp::sys::seed(788897);

    /// strategy lt buys/sells on aveage 1 stocks ecach second on average
    tliquiditytaker lt("lt", 1, 1);

    /// maket maker starts with price 500 and his target inventory is 5000,
    /// the bid and ask size is 1
    tstollmarketmaker mm("mm", 500, 5000, 1,0.01);
    zistrategy zi("zi", 10,0.5,1,100);
    trendist tr("trendist", 1,1);
    simplebuy sb("simplebuy",1,0.1);

    /// MS changed 100 to 10
    tsimulation::addstrategy(mm, {100000,5000});
    tsimulation::addstrategy(lt, {100000,5000});

    tsimulation::addstrategy(zi, {100000,5000});
    tsimulation::addstrategy(sb, {1000,10});
    tsimulation::setlogging(true);


    //        tsimulation::setlogging(true);
    //doba simulacie

    if(evaluate)
    {
        auto r= tsimulation::evaluate(100,10);
        for(unsigned i=0; i<tsimulation::numstrategies(); i++)
            orpp::sys::log() << "Stragegy " << tsimulation::results().tradinghistory[i].name()
                  << ": " << r[i].meanvalue << ", sterr: " << r[i].standarderr << std::endl;
    }
    else
    {
        tsimulation::run(100);
        auto results = tsimulation::results();

        for(auto& r: results.tradinghistory)
        {
            r.output(orpp::sys::log(),ttradinghistory::ewallet);// etrades);
            orpp::sys::log() << "Value: "
                  << r.wallet().money()
                        + results.history.p(std::numeric_limits<double>::max()) *  r.wallet().stocks() << std::endl;
        }

        results.history.output(orpp::sys::log(),1);
    }
}

int main()
{
    try
    {
        martin(false);
        example(true);
        return 0;
    }
    catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    catch (const char* m)
    {
           std::cerr << m << std::endl;
           return 1;
    }
    catch (const std::string& m)
    {
           std::cerr << m << std::endl;
           return 1;
    }
    catch(...)
    {
        std::cerr << "unknown exception" << std::endl;
        return 1;
    }

    return 0;
}



