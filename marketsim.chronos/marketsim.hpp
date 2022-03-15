#ifndef MARKETSIM_HPP
#define MARKETSIM_HPP
#include "assert.h"
#include <vector>
#include <ostream>
#include <limits>
#include <random>
#include <atomic>
#include <algorithm>
#include "orpp.hpp"
#include "Chronos.hpp"
#include "Worker.hpp"

// the namespace encapulating all the library
namespace marketsim
{

class tmarketdata;
using snapshot_shared_ptr = std::shared_ptr<tmarketdata>;


using tprice = int;


static constexpr tprice kmaxprice = std::numeric_limits<tprice>::max()-1;
static constexpr tprice kminprice = 1;
static constexpr tprice khundefprice = std::numeric_limits<tprice>::max();
static constexpr tprice klundefprice = 0;

/// converts \p a into \p tprice and trims is to [\p kminprice, \p kmaxprace]
inline tprice d2ptrim(double a)
{
    return std::min(kmaxprice,std::max(kminprice, static_cast<tprice>( a + 0.5)));
}

using tvolume = int;
using tabstime = double;
using ttimestamp = unsigned long;

//static constexpr ttime kmaxchronostime = std::numeric_limits<ttime>::max();

// converts \p to a string
inline std::string p2str(tprice p)
{
    if(p==klundefprice)
        return "lundef";
    else if(p==khundefprice)
        return "hundef";
    else
    {
        std::ostringstream os;
        os << p;
        return os.str();
    }
}


/// class describing the strategies holdings
class twallet
{
public:
    twallet() : fmoney(0), fstocks(0) {}

    /// constructs new instance with \p amoney of cash and \p astocks of stocks
    twallet(tprice amoney, tvolume astocks) : fmoney(amoney), fstocks(astocks) {}

    /// accessor
    tprice& money() { return fmoney; }
    /// accessor
    tvolume& stocks() { return fstocks; }
    /// accessor
    const tprice& money() const  { return fmoney; }
    /// accessor
    const tvolume& stocks() const { return fstocks; }

    void output(std::ostream& o) const
    {
        o << "m=" << fmoney << ", s=" << fstocks;
    }
private:
    tprice fmoney;
    tvolume fstocks;
};


struct tsettleerror
{
    /// possible warnings, which can be returned back when settling the request
    enum eresult { OK /* never used */ , ecrossedorders, enotenoughmoneytobuy, enotenoughstockstosell,
                    enotenoughmoneytoput, enotenoughstockstoput,
                    enumresults};
    eresult w;
    std::string text;
};


/// Request for an order
struct tpreorder
{
    /// limit price (\c klundef for sell market order, \c khundef for buy market orders)
    tprice price;
    /// ordered/offered volume
    tvolume volume;
    /// constructor
    tpreorder(tprice aprice, tvolume avolume) : price(aprice), volume(avolume)
    {
    }
    /// constructs a buy/sell marketorder with volume \p avolume, buy / sell if \p ask is \c false / \c true
    template <bool ask>
    static tpreorder marketorder(tvolume avolume)
    {
        if constexpr(ask)
            return tpreorder(klundefprice,avolume);
        else
            return tpreorder(khundefprice,avolume);
    }

    /// checks if the order is a market one
    template <bool ask>
    bool ismarket() const
    {
        if constexpr(ask)
        {
            assert(price < khundefprice);
            return price == klundefprice;
        }
        else
        {
            assert(price > klundefprice);
            return price == khundefprice;
        }
    }
};

/// Serves for displaying of \p r by \c iostream
inline std::ostream& operator << (std::ostream& o, const tpreorder& r)
{
    o << "(";
    switch(r.price)
    {
        case klundefprice:
            o << "sell market";
            break;
        case khundefprice:
            o << "buy market";
            break;
        default:
            o << r.price;
    }

    o << "," << r.volume << ")";
    return o;
}

/// Actual order
struct torder : public tpreorder
{
    /// TBD time of submission in market time from simulation start
    tabstime abstime;
    /// an index of strategy owning if, \sa marketsim::tmarket
    ttimestamp timestamp;
    unsigned owner;
    /// constructor
    torder(tprice aprice, tvolume avolume,ttimestamp atimestamp,
                                tabstime aabstime, unsigned aowner)
        : tpreorder(aprice, avolume), abstime(aabstime),
          timestamp(atimestamp), owner(aowner) {}
};

/// operator displaying \p r via \c ostream
inline std::ostream& operator << (std::ostream& o, const torder& r)
{
    o << "(";
    switch(r.price)
    {
        case klundefprice:
            o << "sell market";
            break;
        case khundefprice:
            o << "buy market";
            break;
        default:
            o << r.price;
    }

    o << "," << r.volume << "," << r.abstime << "," << r.owner << ")";
    return o;
}

/// Container of pointers to \p O (have to be compatible with marketsim::tpreorder)
template <typename O>
class torderptrcontainer
{
public:
    torderptrcontainer(const std::vector<torder*>* ax) : fx(ax) {}
    torderptrcontainer() : fx(0) {}
    const O& operator[] (unsigned i) const
    {
        assert(i<fx->size());
        return *(*fx)[i];
    }
    unsigned size() const
    {
        return fx->size();
    }

private:
    const std::vector<O*>* fx;
};

/// \brief Sorted order container.
/// A container of \p O (have to be compatible with marketsim::tpreorder) of type \p ask. The orders are sorted according to FIFO upon inclusion
template <typename O, bool ask>
class tsortedordercontainer
{
public:
    /// serves for sorting
    static bool cmp(const O &a, const O &b)
    {
        if constexpr(std::is_same<O,tpreorder>::value)
        {
            if constexpr(ask)
                return a.price < b.price;
            else
                return a.price > b.price;
        }
        else
        {
            if constexpr(ask)
            {
                if(a.price < b.price)
                    return true;
                else if(a.price > b.price)
                    return false;
                else
                {
                    assert(a.timestamp != b.timestamp);
                    if(a.timestamp < b.timestamp)
                        return true;
                    else
                        return false;
                }
            }
            else
            {
                if(a.price > b.price)
                    return true;
                else if(a.price < b.price)
                    return false;
                else
                {
                    assert(a.timestamp != b.timestamp);
                    if(a.timestamp < b.timestamp)
                        return true;
                    else
                        return false;
                }
            }
        }
    }

    /// the only way to add the order (which is imediatelly put into correct ordering)
    void add(const O& order)
    {
        if constexpr(ask)
        {
           auto it = std::lower_bound( fx.begin(), fx.end(), order , cmp );
           fx.insert( it, order);
        }
        else
        {
            auto it = std::upper_bound( fx.begin(), fx.end(), order , cmp );
            fx.insert( it, order);
        }
    }

    /// removes all orders with zero volume
    void removezeros()
    {
        std::vector<O> newx;
        for( auto r:fx)
        {
            if(r.volume > 0)
                newx.push_back(r);
        }
        fx.clear();
        fx = newx;
    }

    /// accessor
    O& operator [](unsigned i)
    {
        assert(i < fx.size());
        return fx[i];
    }

    /// accessor
    const O& operator [](unsigned i) const
    {
        assert(i < fx.size());
        return fx[i];
    }

    /// returns the number of orders in the list
    unsigned size() const
    {
        return fx.size();
    }

    /// removes all the orders (if \p filter is zero length) or all specified orders
    /// (if \p filter io non-empty, then it removes all orders with \c true in a corresponding
    /// position of \p filter)
    void clear(std::vector<bool> filter = std::vector<bool>())
    {
        if(filter.size()==0)
            fx.clear();
        else
        {
            std::vector<O> newx;
            for(unsigned i=0; i< fx.size(); i++)
            {
                if(i>=filter.size() || filter[i]==false)
                    newx.push_back(fx[i]);
            }
            fx.clear();
            fx = newx;
        }
    }

private:
    std::vector<O> fx;

};

/// An preorder/order list of type \p ask, \p C is either marketsim::torderptrcontainer
/// or marketsim::tsortedordercontainer
template <typename C, bool ask>
class tlistbase : public C
{
public:

    /// constructor
    tlistbase(const C& ac) : C(ac) {}

    /// constructor
    tlistbase()  {}

    /// maximal price of the orders in the list, returns \c klundefprice if the list is empty
    tprice maxprice() const
    {
        if constexpr(ask)
        {
            if(this->size()==0)
                return klundefprice;
            else
                return (*this)[this->size()-1].price;
        }
        else
        {
            if(this->size()==0)
                return klundefprice;
            else
                return (*this)[0].price;
        }
    }

    /// minimal price of the orders in the list, returs \c khundefprice if the list is empty
    tprice minprice() const
    {
        if constexpr(ask)
        {
            if(this->size()==0)
                return khundefprice;
            else
                return (*this)[0].price;
        }
        else
        {
            if(this->size()==0)
                return khundefprice;
            else
                return (*this)[this->size()-1].price;
        }
    }

    /// computes the value (sum of prices times volumes) of the order of in the list
    /// -- if \p filter is empty, then all orders are processed, otherwise only those
    /// with \c true in corresponding positions of \p filter
    tprice value(std::vector<bool> filter = std::vector<bool>()) const
    {
        double s = 0.0;
        for(unsigned i=0; i< this->size(); i++)
        {
            if(filter.size() == 0 || (i<filter.size() && filter[i]))
                s += (*this)[i].volume * (*this)[i].price;
        }
        return s;
    }

    tvolume volume(std::vector<bool> filter = std::vector<bool>()) const
    {
        double s = 0.0;
        for(unsigned i=0; i< this->size(); i++)
        {
            if(filter.size() == 0 || (i<filter.size() && filter[i]))
                s += (*this)[i].volume;
        }
        return s;
    }

    void output(std::ostream& o) const
    {
        for(unsigned i=0; i<this->size(); i++)
            o << (*this)[i] << ",";
    }
};


/// Outputs \p v to stream \p o.
template <typename C, bool ask>
inline std::ostream& operator << (std::ostream& o, const tlistbase<C,ask>& v)
{
    for(unsigned i=0; i<v.size(); i++)
        o << v[i] << ",";
    return o;
}

/// Sorted list of preorders
template <bool ask>
using tpreorderlist=tlistbase<tsortedordercontainer<tpreorder,ask>,ask>;

/// Sorted list of orders
template <bool ask>
using torderlist=tlistbase<tsortedordercontainer<torder,ask>,ask>;

/// List of pointers to orders
template <bool ask>
using torderptrlist=tlistbase<torderptrcontainer<torder>,ask>;

/// Base for order profile (i.e. the list of buy and sell orders)
template <typename LB, typename LA>
class tprofilebase
{
public:
    /// list of buy orders
    LB B;
    /// list of sell orders
    LA A;

    /// accessor
    void setlists(const LB& ab, const LA& aa)
    {
        B = ab;
        A = aa;
    }

    /// sell order with minimal price, \c khundef if empty
    tprice a() const
    {
        return A.minprice();
    }
    /// buy order with maximal price, \c klundef if empty
    tprice b() const
    {
        return B.maxprice();
    }

    /// clears both the lists
    void clear()
    {
        B.clear();
        A.clear();
    }

    /// checks whether the lists are not crossed (i.e. a <= b when both are defined)
    bool check() const
    {
        tprice ap = a();
        tprice bp = b();
        return  a() > b() || (bp == khundefprice && ap==khundefprice) ||
                (bp == klundefprice && ap==klundefprice);
    }

    void output(std::ostream& o) const
    {
        o << "bids: ";
        B.output(o);
        o << std::endl;
        o << "asks: ";
        A.output(o);
        o << std::endl;
    }
};

/// Profile of orders
using torderprofile = tprofilebase<torderlist<false>,torderlist<true>>;

/// Profile of preorders
using tpreorderprofile = tprofilebase<tpreorderlist<false>,tpreorderlist<true>>;

/// Profile of orders, referenced by pointers
using torderptrprofile = tprofilebase<torderptrlist<false>,torderptrlist<true>>;

class tstrategy;

/// Reequest sent to the market
class trequest
{
    friend class tmarket;
    friend class tstrategy;
public:

    /// request to erase (part) of existing profile
    struct teraserequest
    {
        /// constructor
        teraserequest(bool aall) : all(aall) {}

        /// \c true if all orders have to be eradsd, regardless of \c a and \c b,
        /// \c false if \c a and \c b are to be taken into account
        bool all;
        /// \c true if a corresponding order shuld be removed from the \c A list of the profile
        std::vector<bool> a;
        /// \c true if a corresponding order shuld be removed from the \c B list of the profile
        std::vector<bool> b;

        /// \c false if it is certain that nothing is to be erased, \c true if it is possible
        /// that some order will be erased (used to avoid overhead)
        bool possiblyerase() const
        {
            return all || b.size() || a.size();
        }
    };


    /// text version of the warnings, \sa eresult
    static std::string warninglabel(tsettleerror::eresult w)
    {
        static std::vector<std::string> lbls =
        { "OK" , "crossed orders", "not enough money to buy",
          "not enough stocks to sell", "not enough money to put",
          "not enough stocks to put" };
        return lbls[w];
    }

    /// \brief constructor
    /// \param orderrequest orders to be inserted
    /// \param eraserequest which order have to be removed from the existing profile
    /// \param nextupdate after which time the strategy should be called again
    /// (as soon as possible if equal to zero, according to marketsim::tmarkedef::minupdateinterval)
    /// \param consumption amount of money to be consumed
    trequest(const tpreorderprofile& orderrequest,
             const teraserequest& eraserequest = teraserequest(true),
             tprice consumption = 0) :
        fconsumption(consumption),
        forderrequest(orderrequest),
        feraserequest(eraserequest)
    {}
    /// constructs epmty request
    trequest() : fconsumption(0),
        forderrequest(), feraserequest(false) {}
    /// accessor
    tprice consumption() const { return fconsumption; }
    /// accessor
    const tpreorderprofile& orderrequest() const { return forderrequest; }
    /// accessor
    const teraserequest& eraserequest() const { return feraserequest; }

    bool empty() const
    {
        return !feraserequest.possiblyerase()
                && forderrequest.B.size() == 0
                && forderrequest.A.size() == 0;
    }

    void output(std::ostream& o) const
    {
        o << "erase ";
        if(feraserequest.all)
            o << "all" << std::endl;
        else
        {
            o << feraserequest.b.size() << " bids, ";
            o << feraserequest.b.size() << " asks." << std::endl;
        }
        o << "orders" << std::endl;
        forderrequest.output(o);
        o << "consumption: " << fconsumption << std::endl;
    }

private:
    tprice fconsumption;
    tpreorderprofile forderrequest;
    teraserequest feraserequest;
};

/// Class serving strategies (ane later possibly for evaluations) to access the history of market
class tmarkethistory
{
public:
    /// state of the bid and ask values at a time
    struct tsnapshot
    {
        /// bid price (may be \c klundefprice)
        tprice b;
        /// ask price (may be \c khundefprice)
        tprice a;
        /// time of the snapshot
        tabstime t;
        /// computec the midpoint price -- if \c b or \c a are undefined, it returns \c nan
        /// (the return value should be thus checked by \c isnan() )
        double p() const
        {
            if(b==klundefprice || a==khundefprice)
                return std::numeric_limits<double>::quiet_NaN();
            else
                return (a+b) / 2.0;
        }
    };
private:
    static bool cmp(const tsnapshot &a, const tsnapshot &b)
    {
        return a.t < b.t;
    }
public:
    /// constructor
    tmarkethistory() : fx({{klundefprice,khundefprice,0}}) {}

    /// returns the state of the bid and ask at a given time (\p at can be any positive number)
    tsnapshot operator () (tabstime at) const
    {
        tsnapshot val={0,0,at};
        auto it = std::lower_bound(fx.begin(),fx.end(),val,cmp);
        if(it==fx.end())
             it--;
        return *it;
    }

    /// returns the bid at \p at
    tprice b(tabstime at) const
    {
        return (*this)(at).b;
    }

    /// returns the ask at \p at
    tprice a(tabstime at) const
    {
        return (*this)(at).a;
    }

    /// returns the minpoint price at \p at (caution, may be \c nan)
    double p(tabstime at) const
    {
        return (*this)(at).p();
    }

    /// adds a record to the end of the snapshot list (time has to be greater than those already present)
    void add(const tsnapshot& s)
    {
        fx.push_back(s);
    }

    /// displays the history to stream \p o by intervals \p interval
    void output(std::ostream& o, tabstime interval) const
    {
        o << "t,b,a,p" << std::endl;
        for(tabstime t = 0; t < (fx.end()-1)->t + interval; t+= interval )
        {
            auto s = (*this)(t);
            o << t << "," << p2str(s.b) << "," << p2str(s.a) << "," << s.p() << "," << std::endl;
        }
    }
private:
    std::vector<tsnapshot> fx;

};


/// Collects state/history of the strategy's activity. Also holds information about
/// cash and stocks blocked by market.
class ttrading
{
public:
    /// record about consumption
    struct tconsumptionevent
    {
        /// time of the consumption
        tabstime ftime;
        /// amount consumed
        tprice famount;
    };

    /// record about a trade
    struct tradingevent
    {
        /// time of the trade
        tabstime ftime;
        /// increase/descreas of the cash
        tprice fmoneydelta;
        /// increase/decrease of the (stock) inventory
        tvolume fstockdelta;
        /// other party of the trade (index in the market's list)
        unsigned partner;
    };

    /// constructor, \p name is the name of the strategy, \p wallet is the initial endowment
    ttrading(const twallet& wallet) :
        fwallet(wallet), fblockedmoney(0), fblockedstocks(0) {}
    /// accessor
    void addconsumption(tprice c, tabstime t)
    {
        assert(c <= fwallet.money());
        fconsumption.push_back(tconsumptionevent({t,c}));
        fwallet.money() -= c;
    }
    /// accessor
    void addtrade(tprice moneydelta, tvolume stockdelta, tabstime t, unsigned partner)
    {
        assert(fwallet.money() + moneydelta >= 0);
        assert(fwallet.stocks() + stockdelta >= 0);
        ftrading.push_back({t, moneydelta, stockdelta, partner});
        fwallet.money() += moneydelta;
        fwallet.stocks() += stockdelta;
    }
    /// accessor
    const twallet& wallet() const { return fwallet; }
    /// accessor
    twallet& wallet() { return fwallet; }

    /// accessor
    const std::vector<tconsumptionevent>& consumption() const { return fconsumption; }
    /// returns the amount of money blocked by market (to secure active orders)
    const tprice& blockedmoney() const { return fblockedmoney; }
    /// \see blockedmoney
    tprice& blockedmoney() { return fblockedmoney; }
    /// amount of money at disposal (without the blocked ones)
    tprice availablemoney() { return fwallet.money() - fblockedmoney;}
    /// returns the amount of stpcls blocked by market (to secure active orders)
    const tvolume& blockedstocks() const { return fblockedstocks; }
    /// \see blockedstocks
    tvolume& blockedstocks() { return fblockedstocks; }
    /// avaliable (not blocked) amount of stocks
    tvolume availablevolume() { return fwallet.stocks() - fblockedstocks;}
    /// returns trading history
    const std::vector<tradingevent>& trading() const { return ftrading; }
    /// returns the name of the strategy
    const std::string& name() const { return fname; }

    /// indicates degree of detail of the output, \see output
    enum eoutputlevel { enone, ewallet, etrades, enumoutputlevels};

    /// lists the trading history and results to stream \p o at detail level \p level
    void output( std::ostream& o, eoutputlevel level) const
    {
        if(level > enone)
            o << "Results of strategy " << name() << std::endl;

        if(level >= ewallet)
        {
            o << "Money: " << wallet().money()
                             << ", stocks: " << wallet().stocks()
                             << std::endl;
        }
        if(level >= etrades)
        {
            o << "Trades:"  << std::endl;
            for(auto tr: trading())
                o << tr.ftime << "s: " << tr.fmoneydelta << "$, "
                  << tr.fstockdelta << " with " << tr.partner << std::endl;
        }
    }


protected:
    std::string fname;
    twallet fwallet;
    tprice fblockedmoney;
    tvolume fblockedstocks;
    std::vector<tconsumptionevent> fconsumption;
    std::vector<tradingevent> ftrading;
};


class tmarket;
class tmarketinfo;


/// General base class for strategies
class tstrategy: private chronos::Worker
{
    friend class tmarket;
    virtual void main() override
    {
        try {
            trade(fendowment);
        } catch (...) {

        }
    }


protected:
    /// constructor, \p name is recommended to be unique to each instance
    tstrategy(const std::string& name) : fname(name), fmarket(0) {}
//    ~tstrategy() {}
    virtual void trade(twallet /* endowment */) = 0;



public:

    struct trequestresult
    {
        std::vector<tsettleerror> results;
        bool timehorizonreached = false;
    };


    /// accessor
    const std::string& name() const { return fname; }
protected:
    template <bool chronos = true>
    trequestresult request(const trequest& request, tabstime t=0);

    template <bool chronos = true>
    tmarketinfo getinfo();

    tabstime abstime();
    void sleepfor(tabstime t);
    void sleepuntil(tabstime t);
    bool endoftrading();

private:
    std::string fname;
    /// is set by (friend) class tmarket
    twallet fendowment;
    tmarket* fmarket;
};


template <bool chronos>
struct selectstragegybase;

class eventdrivenstrategy;

template<>
struct selectstragegybase<false>
{
    using basetype = eventdrivenstrategy;
};

template<>
struct selectstragegybase<true>
{
    using basetype = tstrategy;
};


template <bool chronos>
class competitorbase
{
public:
    virtual typename selectstragegybase<chronos>::basetype* create() = 0;
};

template<typename S, bool chronos = true>
class competitor : public competitorbase<chronos>
{
public:
    virtual typename selectstragegybase<chronos>::basetype* create()
    {
        return new(S);
    }
};



/// Results of the simulation
class tmarketresult
{
public:
    /// history of the market
    const tmarkethistory& history;
    /// trading histories of individual strategies
    const std::vector<ttrading>& tradinghistory;
};


/// struct defining market behavior
struct tmarketdef
{
    /// mean of (exponential) interval wotj which the market checks new requests
//    double flattency = 0.01;
    /// minimuim time after which the stretegy can be called again
    double minupdateinterval = 0.001;
    /// \p chronos duration
    chronos::app_duration chronosduration = chronos::app_duration(1000);
    /// tbd
    double chronos2abstime = 0.01;
};

class torderbook
{
public:
    torderbook(unsigned n) : fbook(n)
    {
    }


    torderbook duplicate() const
    {
        auto ret(*this);
        ret.sort();
        return ret;
    }

    template <bool ask>
     void makequeue()
     {
         std::vector<torder*>& dst = ask ? fa : fb;
         dst.clear();
         for(unsigned j=0; j<numagents(); j++)
         {
             torderlist<ask>* src;
             if constexpr(ask )
                 src = &fbook[j].A;
             else
                 src = &fbook[j].B;
             src->removezeros();
             unsigned s = src->size();
             for(unsigned i=0; i<s; i++)
                 dst.push_back(&((*src)[i]));
         }
         std::stable_sort(dst.begin(), dst.end(), cmp<ask> );
     }

    void sort()
    {
        makequeue<true>();
        makequeue<false>();
    }


    std::vector<tsettleerror> settle(
                       const trequest& arequest,
                       std::vector<ttrading>& profiles,
                       unsigned owner,
                       ttimestamp ts,
                       tabstime at)
    {
        std::vector<tsettleerror> ret;
        assert(owner < numstrategies());
        const tpreorderprofile& request = arequest.orderrequest();
        if(!request.check())
        {
            ret.push_back({tsettleerror::ecrossedorders,"Crossed orders."});
            return ret;
        }
        const trequest::teraserequest& eraserequest = arequest.eraserequest();
        if(eraserequest.possiblyerase())
        {
            std::vector<bool> afilter, bfilter;
            if(!eraserequest.all)
            {
                afilter = eraserequest.a;
                bfilter = eraserequest.b;
            }
            if(eraserequest.all || afilter.size())
            {
                profiles[owner].blockedstocks() -= fbook[owner].A.volume(bfilter);
                fbook[owner].A.clear(afilter);
                makequeue<true>();
            }
            if(eraserequest.all || bfilter.size())
            {
                profiles[owner].blockedmoney() -= fbook[owner].B.value(afilter);
                fbook[owner].B.clear(bfilter);
                makequeue<false>();
            }
        }

//            if(request.b() >= fbook[owner].a()
//                || request.a() <= fbook[owner].b())
//                return;

        unsigned j = 0;
        for(unsigned i = 0; i<request.B.size(); i++)
        {
            tpreorder r = request.B[i];
            tvolume remains = r.volume;
            for(;remains > 0 && j < fa.size() && fa[j]->price <= r.price; j++)
            {
                tvolume toexec = std::min(remains, fa[j]->volume);
                tprice price = fa[j]->price;
                tvolume available = profiles[owner].availablemoney() / price;
                if(available < toexec)
                {
                    std::ostringstream es;
                    es << "Not enough money to buy " << r << " stocks. Only "
                       << available << " could be bount ";
                    ret.push_back({tsettleerror::enotenoughmoneytobuy,es.str()});
                    toexec = available;
                }
                if(toexec > 0)
                {
                    profiles[owner].addtrade(-price * toexec, toexec, at,fa[j]->owner);
                    profiles[fa[j]->owner].addtrade(price * toexec, -toexec, at, owner);
                    profiles[fa[j]->owner].blockedstocks() -= toexec;
                    fa[j]->volume -= toexec;
                    remains -= toexec;
                }
                else
                    break;
            }

            if(remains && !r.ismarket<false>())
            {
                tvolume available = profiles[owner].availablemoney() / r.price;
                tvolume toput;
                if(available < remains)
                {
                    std::ostringstream es;
                    es << "Not enought money to back bid " << r << ", only "
                       << available << " could be put.";
                    ret.push_back({tsettleerror::enotenoughmoneytoput,es.str()});
                    toput = available;
                }
                else
                    toput = remains;
                fbook[owner].B.add( torder(r.price,toput,ts,at,owner) );
                profiles[owner].blockedmoney() += r.price * toput;
            }
            sort();
        }

        j = 0;
        for(unsigned i = 0; i<request.A.size(); i++)
        {
            tpreorder r = request.A[i];
            tvolume remains = r.volume;

            for(;remains > 0 && j < fb.size() && fb[j]->price >= r.price; j++)
            {
                tvolume toexec = std::min(remains, fb[j]->volume);
                tprice price = fb[j]->price;

                tvolume available = profiles[owner].availablevolume();
                if(available < toexec)
                {
                    std::ostringstream es;
                    es << "Not enough stocks to execute ask of " << r
                       << ", only " << available << " available";
                    ret.push_back({tsettleerror::enotenoughstockstosell,es.str()});
                    toexec = available;
                }

                if(toexec > 0)
                {
                    profiles[owner].addtrade(price * toexec, -toexec, at, fb[j]->owner);
                    profiles[fb[j]->owner].addtrade(-price * toexec, toexec, at, owner);
                    profiles[fb[j]->owner].blockedmoney() -= fb[j]->price * toexec;
                    fb[j]->volume -= toexec;
                    remains -= toexec;

                }
                else
                    break;
            }
            if(remains && !r.ismarket<true>())
            {
                tvolume available = profiles[owner].availablevolume();
                tvolume toput;
                if(available < remains)
                {
                    std::ostringstream es;
                    es << "Not enough stocks to put ask of " << r << ", only " << available << " available";
                    ret.push_back({tsettleerror::enotenoughstockstoput,es.str()});
                    toput = available;
                }
                else
                    toput = remains;
                fbook[owner].A.add( torder(r.price,toput,ts,at,owner) );
                profiles[owner].blockedstocks() += toput;
                makequeue<true>();
            }
            sort();
        }

        return ret;
    }

    unsigned numstrategies() const { return fbook.size(); }

    const torderprofile& profile(unsigned i) const
    {
        return fbook[i];
    }



//    const torderlist<torder>& aprofile(unsigned i) const { return profile(i).A; }
//    const torderlist<torder>& bprofile(unsigned i) const { return profile(i).B; }


    tprice a() const
    {
        if(fa.size())
            return fa[0]->price;
        else
            return khundefprice;
    }
    tprice b() const
    {
        if(fb.size())
            return fb[0]->price;
        else
            return klundefprice;
    }


    torderptrprofile obprofile()
    {
        torderptrprofile ret;
        torderptrlist<false> bl(&fb);
        torderptrlist<true> al(&fa);
        ret.setlists(bl,al);
        return ret;
    }

    std::vector<torderprofile> fbook;
    unsigned numagents() const
    {
        return fbook.size();
    }

    std::vector<torder*> fa;
    std::vector<torder*> fb;

    template <bool ask>
    static bool cmp(const torder *a, const torder *b)
    {
        return tsortedordercontainer<torder,ask>::cmp(*a,*b);
    }
};

struct statcounter
{
    double sum = 0.0;
    double sumsq = 0.0;
    unsigned num = 0;
    void add(double x) { sum += x; sumsq += x*x; num++;}
    double average() { return sum / num; }
};

class tmarketdata
{
    static std::vector<ttrading> maketradings(const std::vector<twallet>& endowments)
    {
        std::vector<ttrading> ret;
        for(unsigned i=0; i<endowments.size(); i++)
            ret.push_back(ttrading(endowments[i]));
        return ret;
    }
public:
    tmarketdata(const std::vector<std::shared_ptr<tstrategy>> strategies,
          const std::vector<twallet>& endowments)
       :  fstrategies(strategies),
          ftradings(maketradings(endowments)),
          forderbook(endowments.size()),
          ftimestamp(0)
    {}
    tmarketdata(const tmarketdata& src) :
        fstrategies(src.fstrategies),
        ftradings(src.ftradings),
        fhistory(src.fhistory),
        forderbook(src.forderbook.duplicate()),
        ftimestamp(src.ftimestamp),
        fremainingdurations(src.fremainingdurations)
    {
    }
    const std::vector<std::shared_ptr<tstrategy>> fstrategies;
    std::vector<ttrading> ftradings;
    tmarkethistory fhistory;
    torderbook forderbook;
    ttimestamp ftimestamp;
    statcounter fremainingdurations;
    unsigned n() const { return ftradings.size(); }
};


class tmarketinfo
{
    snapshot_shared_ptr
    fdata;
public:
    tmarketinfo(const // std::shared_ptr<tmarketdata>
             snapshot_shared_ptr data, unsigned id, tabstime at):
        fdata(data), history(data->fhistory),orderbook(data->forderbook.obprofile()),
        myprofile(data->forderbook.profile(id)), myid(id), t(at)
    {
    }

    /// history of the market
    const tmarkethistory& history;
    /// current state of the order book
    const torderptrprofile orderbook;
    /// the profile belonging to the target strategy
    const torderprofile& myprofile;
    /// id of the target strategy in the markat's list (corresponds to marketsim::torder::owner)
    unsigned myid;
    /// current time
    tabstime t;

    tprice a() const { return orderbook.A.minprice(); }
    tprice b() const { return orderbook.B.maxprice(); }
    tprice alpha() const
    {
       for(unsigned i=0; i<orderbook.A.size(); i++)
            if(orderbook.A[i].owner != myid)
                return orderbook.A[i].price;
       return khundefprice;
    }

    tprice beta() const
    {
       for(unsigned i=0; i<orderbook.B.size(); i++)
            if(orderbook.B[i].owner != myid)
                return orderbook.B[i].price;
       return klundefprice;
    }
};

class randomizingstrategy:  public tstrategy
{
protected:
    std::default_random_engine fengine;
    std::uniform_real_distribution<double> funiform;
    randomizingstrategy(const std::string& name) : tstrategy(name)
    {
        fengine.seed(0 /* time(0) */ );
    }
    double uniform()
    {
        return funiform(fengine);
    }
public:
    void seed(unsigned i)
    {
        fengine.seed(i+1);
    }
};

class eventdrivenstrategy : public randomizingstrategy
{
    friend class tmarket;


public:
    eventdrivenstrategy(const std::string& name,
                         double interval, bool random = false) :
        randomizingstrategy(name),
        finterval(interval), frandom(random), fnu(interval) {}
    virtual trequest event(const tmarketinfo& info) = 0;
    virtual void trade(twallet) override
    {
        while(!endoftrading())
        {
            tmarketinfo info = this->getinfo<true>();
            request<true>(event(info));
            double t = frandom ? fnu(fengine) : finterval;
            sleepuntil(info.t+t);
        }
    }

    tabstime step(tabstime t)
    {
        auto info = getinfo<false>();
        request<false>(event(info),t);
        return t + (frandom ? fnu(fengine) : finterval);
    }
private:
    double finterval;
    bool frandom;
    std::exponential_distribution<> fnu;
};


/// Class performing the simulation. Need not be instantiated, all methods are static
/// (has to be only single one in program, however)
class tmarket : private chronos::Chronos
{

private:
    friend class tstrategy;

    tabstime getabstime()
    {
        return this->get_time() * fdef.chronos2abstime;
    }


    /// tbd add special exception, as client may misspecify its pointer;
    int findstrategy(const tstrategy* ownerptr)
    {
        assert(fmarketdata);
        for(unsigned i=0; i<fmarketdata->fstrategies.size(); i++)
            if(ownerptr == fmarketdata->fstrategies[i].get())
                return i;
        throw "Internal error: cannot assign owner";
    }

    tmarketinfo getinfo(const tstrategy* ownerptr, tabstime t)
    {
        int owner = findstrategy(ownerptr);
        if(flog)
        {
            *flog << "Called getinfo by " << ownerptr->name()
                  << "(" << owner << ")"
                  << " at " << getabstime() << "(" << this->get_time() << ")"
                  << std::endl;

        }

        if(flog)
            *flog << std::endl << std::endl;
//        std::shared_ptr<tmarketdata>
        snapshot_shared_ptr        ssht = atomic_load(&fmarketsnapshot);
        assert(ssht);
        return tmarketinfo(ssht,owner,t);

    }

    template <bool chronos>
    std::vector<tsettleerror> settle(const tstrategy* ownerptr,
                                     const trequest& request,
                                     tabstime t = 0)
    {
        int owner = findstrategy(ownerptr);
        if(flog)
        {
            *flog << "Called settle by " << ownerptr->name()
                  << "(" << owner << ")"
                  << " at " << getabstime() << "(" << this->get_time() << ")"
                  << std::endl;
            *flog << "Request: ";
            request.output(*flog);
            *flog << "Timestamp: " << fmarketdata->ftimestamp << std::endl;
            *flog << std::endl;
        }
        auto& ob = fmarketdata->forderbook;
        auto st = chronos ? t : getabstime();
        std::vector<tsettleerror> errs = ob.settle(
                       request,
                       fmarketdata->ftradings,
                       owner,
                       fmarketdata->ftimestamp++,
                       st);

        if(flog)
        {
            *flog << "Result of settle ";
            if(errs.size()==0)
                *flog << "OK";
            else
                for(unsigned i=0; i<errs.size(); i++)
                    *flog << std::endl << errs[i].text;

            *flog << std::endl << std::endl;

            *flog << "Wallet: ";
            auto t = fmarketdata->ftradings[owner];
            t.wallet().output(*flog);
            *flog << std::endl;
            *flog << "Blocked: m=" << t.blockedmoney() <<
                     " s=" << t.blockedstocks() << std::endl;

            *flog << "Resulting order book: " << std::endl;
            fmarketdata->forderbook.obprofile().output(*flog);

            *flog << std::endl << std::endl;
        }
        fmarketdata->fhistory.add({ob.b(),ob.a(),st});
        return errs;

    }

    void setsnapshot()
    {
        assert(fmarketdata);
        if(flog)
            *flog << "Creating a new copy of tmarketdata... ";
        atomic_store(&fmarketsnapshot, std::make_shared<tmarketdata>(*new tmarketdata(*fmarketdata)));
        if(flog)
            *flog << "done!" << std::endl;
    }

    virtual void tick() override
    {
        assert(fmarketdata);
        setsnapshot();


        fmarketdata->fremainingdurations.add(get_remaining_time().count());
    }


public:

    tmarket(tabstime maxtime, tmarketdef adef  = tmarketdef()) :
        chronos::Chronos(adef.chronosduration, maxtime / adef.chronos2abstime), fdef(adef)
    {
    }


    /// if calles with \p alogging=\c true, detailed information is sent to
    /// flog during simulation
    void setlogging(std::ostream& log)
    {
        flog = &log;
    }

    void resetlogging()
    {
        flog = 0;
    }
    /// changes the simulation definition

    /// accessor
    const tmarketdef& def() const
    {
        return fdef;
    }

    /// \c true if logging is on
    bool islogging() const
    {
        return flog != 0;
    }

    /// returns results of the last simulation
    std::shared_ptr<tmarketdata> results() const
    {
        return fmarketdata;
    }

    /// Run the simulation of \p timeofrun seconds of trading (the simulation is usually much shorter).
    template <bool chronos=true>
    void run(std::vector<competitorbase<chronos>*> competitors,
             std::vector<twallet> endowments)
    {
        assert(endowments.size()==competitors.size());
        if(flog)
            *flog << "Starting simulation with " << endowments.size()
                           << " strategies" << std::endl;

        std::vector<std::shared_ptr<tstrategy>> strategies;
        for(unsigned i=0; i<competitors.size(); i++)
            strategies.push_back(std::shared_ptr<tstrategy>(competitors[i]->create()));

        fmarketdata.reset(new tmarketdata(strategies,endowments));
        setsnapshot();
        chronos::workers_list wl;
        for(unsigned i=0; i<strategies.size(); i++)
        {
            wl.push_back(strategies[i].get());
            strategies[i]->fendowment = endowments[i];
            strategies[i]->fmarket = this;
        }
        if constexpr(chronos)
        {
            chronos::Chronos::run(wl);
            chronos::Chronos::wait() ;
        }
        else
        {
            auto n = competitors.size();
            tabstime T = get_max_time() * def().chronos2abstime;
            std::vector<tabstime> ts(n,0);
            unsigned egen = 0;
for(unsigned i=0; i<n; i++)
    static_cast<eventdrivenstrategy*>(strategies[i].get())->seed(i);
            for(;;)
            {
                unsigned first;
                tabstime t = std::numeric_limits<tabstime>::max();
                for(unsigned i=0; i<n; i++)
                {
                    if(ts[i] <= t)
                    {
                        t = ts[i];
                        first = i;
                    }
                }
                if(t >= T)
                    return;
                ts[first]=(static_cast<eventdrivenstrategy*>(strategies[first].get()))->step(t);
                setsnapshot();
            }
        }

    }

    struct averageresult
    {
        double meanvalue;
        double standarderr;
    };

    /// Evaluates the current strategies by running \p nruns simulation of length \p timeofrun.
    /// For each strtategy it returns mean value of its profit (comparison to the "hold" strategy,
    /// i.e. doing nothing) and the standard deviation.

/*    std::vector<averageresult> evaluate(double timeofrun, unsigned nruns, bool countremainingstocks = true)
    {
        std::vector<double> s(numstrategies(),0);
        std::vector<double> s2(numstrategies(),0);
        unsigned nobs = 0;
        for(unsigned i=0; i<nruns; i++)
        {
            flog << i  << std::endl;
            run(timeofrun);

            for(unsigned j=0; j<numstrategies(); j++)
            {
               auto& r= results().tradinghistory[j];
               double p = results().history.p(std::numeric_limits<double>::max());
               auto& e = self().fendowments[j];
               double v = (r.wallet().money() - e.money());
               for(unsigned k=0; k<r.consumption().size(); k++)
                   v += r.consumption()[k].famount;
               if(countremainingstocks)
               {
                   if(isnan(p))
                       flog << "nan result" << std::endl;
                   else
                         v += p *  (r.wallet().stocks() - e.stocks());
               }
               s[j]+=v;
               s2[j]+=v*v;
               nobs++;
            }
        }
        std::vector<averageresult> ret;
        for(unsigned j=0; j<numstrategies(); j++)
        {
            double m=s[j] / nobs;
            ret.push_back({m, sqrt((s2[j] - m*m))/nobs});
        }
        return ret;
    } */

private:

    tmarketdef fdef;
    std::ostream* flog = 0;

//    std::vector<tstrategy*> fstrategies;
    std::shared_ptr<tmarketdata> fmarketdata;
//    std::atomic<std::shared_ptr<tmarketdata>>
    snapshot_shared_ptr fmarketsnapshot;

};


/*
/// Simplified version of \ref tstrategy. The simplifiaction consists in once forever
/// set interval of update (can be however zero, which means instanteneous update) and
/// that only four orders can be sent to the market at a time (two market-- and two limit ones,
/// each of a different type). Existing orders are erased if the requested ones do not match them).
class tsimplestrategy : public tstrategy
{
public:
    /// struct that should be sent to the market
    struct tsimpleorderprofile
    {
        tpreorder b = tpreorder(klundefprice,0);
        tpreorder a = tpreorder(khundefprice,0);
        tvolume mb = 0;
        tvolume ma = 0;
        tprice consumption = 0;
    };

    /// returns a \ref tsimpleorderprofile corresponding to a single buy market order
    static tsimpleorderprofile buymarket(tvolume volume)
    {
        tsimpleorderprofile s;
        s.mb = volume;
        return s;
    }

    /// returns a \ref tsimpleorderprofile corresponding to a single sell market order
    static tsimpleorderprofile sellmarket(tvolume volume)
    {
        tsimpleorderprofile s;
        s.ma = volume;
        return s;
    }

    /// returns a \ref tsimpleorderprofile meaning that no order should be submitted (and all
    /// existing erased)
    static tsimpleorderprofile noorder()
    {
        tsimpleorderprofile s;
        return s;
    }
    /// accessor
    ttime timeinterval() const { return finterval; }
protected:
    /// constructor; \p name name, \p timeinterval time interval between market actions
    /// (if zero then called asap), \p random determines, whether the next update should be
    /// deterministit (false) or exponential with expectation \p timeinterval (hence intensity
    /// 1/timeinterval.
    tsimplestrategy(const std::string& name, ttime timeinterval, bool randominterval=false)
        : tstrategy(name), finterval(timeinterval), frandom(randominterval)
    {        assert(finterval != 0 || !frandom);
}
private:
    /// event handler which has to be overriden by any accessor, \p mi reports
    /// the state of the market, \p history the history associated with the strategy
    virtual tsimpleorderprofile simpleevent(const tmarketinfo& mi,
                                     const ttrading& history ) = 0;

    virtual trequest event(const tmarketinfo& mi,
                                  const ttrading& aresult )
    {

        tsimpleorderprofile r = simpleevent(mi, aresult) ;
        trequest::teraserequest er(false);
        tpreorderprofile pp;
        unsigned numb = mi.myprofile.B.size();
        assert(numb <= 1);
        unsigned numa = mi.myprofile.A.size();
        assert(numa <= 1);

        if(r.mb > 0)
            pp.B.add(torder::marketorder<false>(r.mb));
        if(r.ma > 0)
            pp.A.add(torder::marketorder<true>(r.ma));

        bool keepb = false;
        if(r.b.volume)
        {
            if(numb > 0
                    && mi.myprofile.B[0].price==r.b.price
                    && mi.myprofile.B[0].volume==r.b.volume)
                keepb = true;
            else
                pp.B.add(r.b);
        }
        bool keepa = false;
        if(r.a.volume)
        {
            if(numa > 0
                    && mi.myprofile.A[0].price==r.a.price
                    && mi.myprofile.A[0].volume==r.a.volume)
                keepa = true;
            else
                pp.A.add(r.a);
        }
        if(numa && !keepa)
            er.a.push_back(true);
        if(numb && !keepb)
            er.b.push_back(true);
        ttime next;
        if(frandom)
        {
            std::exponential_distribution<> nu(1.0 / finterval);
            next = nu(orpp::sys::engine());
        }
        else
            next = finterval;
        return trequest(pp,er,next,r.consumption);
    }
    ttime finterval;
    bool frandom;
};

*/

template <bool chronos>
inline tstrategy::trequestresult tstrategy::request(const trequest& request, tabstime t)
{
    assert(fmarket);
    tstrategy::trequestresult ret;
    try
    {
        if(!request.empty())
        {
            if constexpr(chronos)
            {
                ret.results = fmarket->async([this,&request]()
                {
                    return this->fmarket->settle<true>(this,request);
                });
            }
            else
                ret.results = this->fmarket->settle<false>(this,request,t);
        }
        ret.timehorizonreached = false;
        return ret;
    }
    catch (chronos::error_already_finished)
    {
        ret.timehorizonreached = true;
        return ret;
    }
    catch (chronos::error& e)
    {
        throw e.what();
    }
    catch (...)
    {
        throw "Unknown error of Chronos";
    }
}

template <bool chronos>
tmarketinfo tstrategy::getinfo()
{
    assert(fmarket);
    try
    {
        return this->fmarket->getinfo(this, 0);
    }
    catch (...)
    {
        throw "unkonwn error in chronos, cought in getinfo.";
    }
}

inline void tstrategy::sleepfor(tabstime t)
{
//    if(fmarket->flog)
//        *(fmarket->flog) << "Called sleepfor t=" << t
//              << " (" << t/fmarket->def().chronos2abstime << ")"
//              << std::endl << std::endl << std::endl;
    sleep_until(fmarket->get_time() + t/fmarket->def().chronos2abstime);
}

inline void tstrategy::sleepuntil(tabstime t)
{
//    if(fmarket->flog)
//        *(fmarket->flog) << "Called sleepuntil t=" << t
//              << " (" << t/fmarket->def().chronos2abstime << ")"
//            << std::endl << std::endl << std::endl;
    sleep_until(t/fmarket->def().chronos2abstime);
}

inline tabstime tstrategy::abstime()
{
    auto result = fmarket->get_time();
    tabstime at = result * fmarket->def().chronos2abstime;
//    if(fmarket->flog)
//        *(fmarket->flog) << "Called abstime, result t=" << at
//              << " (" << result << ")"
//              << std::endl << std::endl << std::endl;
    return at;
}


inline bool tstrategy::endoftrading()
{
    return !ready();
}


} // namespace

#endif // MARKETSIM_HPP
