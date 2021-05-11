#ifndef STRATEGIES_HPP
#define STRATEGIES_HPP

#include "marketsim.hpp"

namespace marketsim
{

/// A liquidity taker strategy, issuing, at a predefined Poisson rate, market orders of a random type.
class tliquiditytaker: public tsimplestrategy
{
public:
    /// constructor, \p interval is the average interval of arrival of the orders (i.e. the mean
    /// time between order is \c 1/intensity), \p volume is the ordered/offered volume of all the orders
    tliquiditytaker(const std::string& name,
            double interval, tvolume volume)
        : tsimplestrategy(name,interval, /* random=*/ true), fvolume(volume) {}
    virtual tsimpleorderprofile simpleevent(const tmarketinfo&,
                                     const ttradinghistory&)
    {
        bool buy = orpp::sys::uniform() < 0.5;
        return buy ? buymarket(fvolume) : sellmarket(fvolume);
    }
private:
    tvolume fvolume;
};

/// Strategy rouhly mimcking the market making strategy described by Stoll (1978)
class tstollmarketmaker: public tsimplestrategy
{
public:
    /// constructor
    /// \param name name
    /// \param initialprice midpoint price set in the beginning when there is no history
    /// \param desiredinventory the target value of the stock
    /// \param offeredvolume volume at bid and ask
    /// \param pestalpha exponential smoothing parameter for estimation of the "true" price
    /// \param dif2bias constant tranfering imbalance to deviation of midpoint and the fair price
    /// \param dif2percspread constant tranfering imbalance to spread
    tstollmarketmaker(const std::string& name,
             tprice initialprice,
             tvolume desiredinventory,
             tvolume offeredvolume = 1,
             double pestalpha = 0.1,

             double dif2bias = 0.005,
             double dif2percspread = 0.005
             )
        : tsimplestrategy(name, 0),
          finitialprice(initialprice),
          fofferedvolume(offeredvolume),
          fdesiredinventory(desiredinventory),
          fdif2percspread(dif2percspread),
          fdif2bias(dif2bias),
          fpestalpha(pestalpha)
             {}
private:
    virtual tsimpleorderprofile simpleevent(const tmarketinfo& mi,
                                     const ttradinghistory& th)
    {
        double dt0 = log(0.001) / log(1-fpestalpha);
        auto lat = tsimulation::def().flattency;
        double startt = floor(mi.t - dt0 * lat);
        double fairprice;
        if(startt <= 0 || isnan(mi.history.p(startt)))
            fairprice = finitialprice;
        else
            fairprice = mi.history.p(startt);
        for(double t = startt; t <= mi.t; t+=lat)
        {
            if(t > 0 && !isnan(mi.history.p(t)))
                fairprice = (1-fpestalpha) * fairprice
                        + fpestalpha * mi.history.p(t);
        }

        double D = fairprice * (th.wallet().stocks() - fdesiredinventory);
        double midpoint = fairprice - fdif2bias * D;
        double spread = std::max(1.0,fdif2percspread * fabs(D));
        tprice a = d2ptrim(midpoint + spread / 2);
        tprice b = d2ptrim(midpoint - spread / 2);

        if(a==b)
        {
            if(a==kminprice)
                a++;
            else if(a==kmaxprice)
                b--;
            else
                throw "zero spread out of edges";
        }

        if(tsimulation::islogging())
            orpp::sys::log() << '\t' << "fp: " << fairprice << ", mp: " << midpoint
                       << " spread: " << spread << " pbias:" << fdif2bias* D << std::endl ;

        tsimpleorderprofile ret;
        ret.a.price = a;
        ret.b.price = b;
        ret.a.volume = ret.b.volume = fofferedvolume;
        return ret;
    }
    tprice finitialprice;
    tvolume fofferedvolume;
    tvolume fdesiredinventory;
    double fdif2percspread;
    double fdif2bias;
    double fpestalpha;
};

// Zero intelligence strategy as of Smith et. al (2003). TBD
class zistrategy: public tstrategy
{
public:
    zistrategy( const std::string& name, double arrivalrate,  double cancelationrate,
                unsigned minprice = 1, unsigned maxprice = 100) :
        tstrategy(name),
        farrivalrate(arrivalrate),
        fcancelationrate(cancelationrate),
        fmaxprice(maxprice),
        fminprice(minprice)
    {}


private:
    double farrivalrate;
    double fcancelationrate;
    int fmaxprice;
    int fminprice;

    virtual trequest event(const tmarketinfo& mi,
                           const ttradinghistory& /* aresult */)
    {
        unsigned numa = mi.myprofile.A.size();
        unsigned numb = mi.myprofile.B.size();
        double totalint = (numa + numb) * fcancelationrate + 2*farrivalrate;
        double u = orpp::sys::uniform() * totalint;

        trequest::teraserequest er(false);
        tpreorderprofile rp;


        if(u< numa * fcancelationrate)
        {
            er.a = std::vector<bool>(numa,false);
            er.a[u / fcancelationrate] = true;
        }
        else
        {
            u -= numa * fcancelationrate;
            if(u< numb * fcancelationrate)
            {
                er.b = std::vector<bool>(numb,false);
                er.b[u / fcancelationrate] = true;
            }
            else
            {
                u -= numb * fcancelationrate;
                double v = orpp::sys::uniform() * (fmaxprice - fminprice + 1);
                unsigned num = orpp::sys::uniform() < 0.5 ? 1 : 2;
                if(u < farrivalrate)
                    rp.A.add( tpreorder(fminprice + static_cast<tprice>(v),num));
                else
                    rp.B.add( tpreorder(fminprice + static_cast<tprice>(v),num));
            }
        }
        std::exponential_distribution<> nu(totalint);
          //  a bit unprecise, should be corrected for present cancellations / insertions
        double nextupdate = nu(orpp::sys::engine());

        return trequest(rp,er,nextupdate);
    }


    virtual void atend(const ttradinghistory&)
    {

    }

    virtual void warning(trequest::ewarning, const std::string&)
    {
    }
};



}

#endif // STRATEGIES_HPP
