#include <boost/math/distributions.hpp>
#include <vector>
#include <iostream>
#include <limits>
#include <fstream>
#include <algorithm>
#include <random>

#include "strategies.hpp"
//#include "peter.hpp"


using namespace marketsim;

class tnaiverketmaker : public tsimplestrategy
{
    using Tvec = std::vector<double>;
    using T2vec = std::vector<Tvec>;
    using T3vec = std::vector<T2vec>;
    using T4vec = std::vector<T3vec>;

public:
    tnaiverketmaker(const std::string& name,
        tprice initprice
    )
        : tsimplestrategy(name, 0),
        finitprice(initprice),
        last_bid(klundefprice),
        last_ask(khundefprice)
    {

    }
private:
    virtual tsimpleorderprofile simpleevent(const tmarketinfo& mi, const ttradinghistory& th)
    {
        double p = isnan(mi.history.p(mi.t)) ? finitprice : mi.history.p(mi.t);

        //update bid and ask (alpha, beta)
        tprice alpha = mi.orderbook.A.minprice();
        tprice beta = mi.orderbook.B.maxprice();

        if (beta == klundefprice) beta = (last_bid == klundefprice) ? p - 1 : last_bid;
        if (alpha == khundefprice) alpha = (last_ask == khundefprice) ? p + 1 : last_ask;

        //calculate v as the expected value
        tprice a_best = alpha, b_best = beta;

        if (alpha - beta == 1)
        {
            if (p >= alpha)
                b_best = beta + 1;
            else if (p <= beta)
                a_best = alpha - 1;
            else
            {
                if (orpp::sys::uniform() < 0.5)
                    a_best = alpha - 1;
                else
                    b_best = beta + 1;
            }
        }
        else
        {
            b_best = b_best + 1;
            a_best = a_best + 1;
        }


        last_ask = a_best;
        last_bid = b_best;

        tsimpleorderprofile ord;
        ord.a.volume = 1;
        ord.b.volume = 1;
        ord.a.price = a_best;
        ord.b.price = b_best;
        return ord;
    }

    tprice finitprice;
    tprice last_bid, last_ask;
};


class simplebuy : public tsimplestrategy
{
public:
    simplebuy(const std::string& name, tvolume delta, ttime timeinterval) :
        tsimplestrategy(name, timeinterval, false),
        fdelta(delta)
    {}
private:
    virtual tsimpleorderprofile simpleevent(const tmarketinfo&,
        const ttradinghistory&)
    {
        return buymarket(fdelta);
    }
    tvolume fdelta;
};

class trendist : public tsimplestrategy
{
public:
    trendist(const std::string& name, tvolume delta, ttime timeinterval)
        : tsimplestrategy(name, timeinterval), fdelta(delta)
    {}
private:
    virtual tsimpleorderprofile simpleevent(const tmarketinfo& mi,
        const ttradinghistory&)
    {
        if (mi.t > 2 * timeinterval())
        {
            double p0 = mi.history.p(mi.t);
            double p1 = mi.history.p(mi.t - timeinterval());
            if (!isnan(p0) && !isnan(p1) && fabs(p0 - p1) > std::numeric_limits<double>::epsilon())
                return p0 > p1 ? buymarket(fdelta) : sellmarket(fdelta);
        }
        return noorder();
    }
    tvolume fdelta;
};

//ucastnici trhu


void martin()
{
    bool evaluate = true;
    // orpp::sys::seed(788897);

    /// strategy lt buys/sells on aveage 1 stocks ecach second on average
    tliquiditytaker lt("lt", 1, 1);

    /// maket maker starts with price 500 and his target inventory is 5000,
    /// the bid and ask size is 1
    tnaiverketmaker naivemm("naivemm", 100);
    tadpmarketmaker adpmm("adpmm", 100, 2000, 100);
    tsimulation::addstrategy(naivemm, { 1000,50 });
    tsimulation::addstrategy(lt, { 1000,50 });
    tsimulation::addstrategy(adpmm, { 1000,50 });

    //        zistrategy zi("zi", 10,0.5,1,100);
    //        trendist tr("trendist", 1,1);
    //        simplebuy sb("simplebuy",1,0.1);
    //        tsimulation::addstrategy(zi, {100000,5000});
    //        tsimulation::addstrategy(sb, {1000,10});
    //        tsimulation::setlogging(true);


    //        tsimulation::setlogging(true);
    //doba simulacie

    if (evaluate)
    {
        auto r = tsimulation::evaluate(100, 10);
        for (unsigned i = 0; i < tsimulation::numstrategies(); i++)
            orpp::sys::log() << "Stragegy " << tsimulation::results().tradinghistory[i].name()
            << ": " << r[i].meanvalue << ", sterr: " << r[i].standarderr << std::endl;
    }
    else
    {
        tsimulation::run(100);
        auto results = tsimulation::results();

        for (auto& r : results.tradinghistory)
        {
            r.output(orpp::sys::log(), ttradinghistory::ewallet);// etrades);
            orpp::sys::log() << "Value: "
                << r.wallet().money()
                + results.history.p(std::numeric_limits<double>::max()) * r.wallet().stocks() << std::endl;
        }

        results.history.output(orpp::sys::log(), 1);
    }
}

int main()
{
    try
    {
        martin();
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
    catch (...)
    {
        std::cerr << "unknown exception" << std::endl;
        return 1;
    }

    return 0;
}