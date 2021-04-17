#include <boost/math/distributions.hpp>
#include <vector>
#include <iostream>
#include <limits>
#include <fstream>
#include <algorithm>
#include <random>

#include "marketsim.hpp"

using namespace marketsim;

class simplebuy: public tsimplestrategy
{
public:
    simplebuy(const std::string& name,tvolume delta, ttime timeinterval) :
        tsimplestrategy(name, timeinterval),
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



int main()
{

    try
    {
        orpp::sys::seed(788897);
        zistrategy zi("zi", 10,0.5,1,100);
        mmstrategy mm("mm");
        trendist tr("trendist", 1,1);
//        mmstrategy         mm2;
        tsimulation::addstrategy(mm, {100000,5000});
//        tsimulation::addstrategy(mm2, {100000,5000});
//        simplebuy sb(1,0.1);
        tsimulation::addstrategy(zi, {100000,5000});
        tsimulation::addstrategy(tr, {1000,10});
        tsimulation::setlogging(false);
        tsimulation::run(20);
        auto results = tsimulation::results();

        for(auto& r:  results.tradinghistory)
            r.output(orpp::sys::log(),ttradinghistory::etrades);

        results.history.output(orpp::sys::log(),1);
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
