#ifndef MARKETSIM_HPP
#define MARKETSIM_HPP
#include "assert.h"
#include <vector>
#include <iostream>
#include <limits>
#include <random>
#include <algorithm>
#include "orpp.hpp"



// the namespace encapulating all the library
namespace marketsim
{

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
using ttime = double;
static constexpr ttime kmaxtime = std::numeric_limits<ttime>::max();

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


/// struct defining market behafior
struct tmarketdef
{
    /// mean of (exponential) interval wotj which the market checks new requests
    double flattency = 0.01;
    /// minimuim time after which the stretegy can be called again
    double minupdateinterval = 0.001;
};

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
private:
    tprice fmoney;
    tvolume fstocks;
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
    /// time of submission in sec from simulation start
    ttime time;
    /// an index of strategy owning if, \sa marketsim::tsimulation
    unsigned owner;
    /// constructor
    torder(tprice aprice, tvolume avolume, ttime atime, unsigned aowner)
        : tpreorder(aprice, avolume), time(atime), owner(aowner) {}
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

    o << "," << r.volume << "," << r.time << "," << r.owner << ")";
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
                else if(a.time < b.time)
                    return true;
                else
                    return false;
            }
            else
            {
                if(a.price > b.price)
                    return true;
                else if(a.price < b.price)
                    return false;
                else if(a.time < b.time)
                    return true;
                else
                    return false;
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

/// An preorder/order list of type \p ask, \p C is either marketsim::torderptrcontainer or marketsim::tsortedordercontainer
template <typename C, bool ask>
class tlistbase : public C
{
public:

    /// constructor
    tlistbase(const C& ac) : C(ac) {}

    /// constructor
    tlistbase()  {}

    /// maximal price of the orders in the list, returs \c klundefprice if the list is empty
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

/// Base for order profile (i.e. the list of bost buy and sell orders)
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
};

/// Profile of orders
using torderprofile = tprofilebase<torderlist<false>,torderlist<true>>;

/// Profile of preorders
using tpreorderprofile = tprofilebase<tpreorderlist<false>,tpreorderlist<true>>;

/// Profile of orders, referenced by pointers
using torderptrprofile = tprofilebase<torderptrlist<false>,torderptrlist<true>>;

/// Reequest sent to the market
class trequest
{
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

    /// possible warnings, which can be returned back when settling the request
    enum ewarning { OK, ecrossedorders, enotenoughmoneytobuy, enotenoughstockstosell,
                    enotenoughmoneytoput, enotenoughstockstoput,
                    enumwargnings};

    /// text version of the warnings, \sa ewarning
    static std::string warninglabel(ewarning w)
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
             ttime nextupdate = 0,
             tprice consumption = 0) :
        fconsumption(consumption),
        fnextupdate(nextupdate),
        forderrequest(orderrequest),
        feraserequest(eraserequest)
    {}
    /// constructs epmty request
    trequest() : fconsumption(0), fnextupdate(0),
        forderrequest(), feraserequest(false) {}
    /// accessor
    tprice consumption() const { return fconsumption; }
    /// accessor
    const tpreorderprofile& requestprofile() const { return forderrequest; }
    /// accessor
    const teraserequest& eraserequest() const { return feraserequest; }
    /// accessor
    ttime nextupdate() const { return fnextupdate; }
public:
    tprice fconsumption;
    ttime fnextupdate;
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
        ttime t;
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
    tsnapshot operator () (ttime at) const
    {
        tsnapshot val={0,0,at};
        auto it = std::lower_bound(fx.begin(),fx.end(),val,cmp);
        if(it==fx.end())
             it--;
        return *it;
    }

    /// returns the bid at \p at
    tprice b(ttime at) const
    {
        return (*this)(at).b;
    }

    /// returns the ask at \p at
    tprice a(ttime at) const
    {
        return (*this)(at).a;
    }

    /// returns the minpoint price at \p at (caution, may be \c nan)
    double p(ttime at) const
    {
        return (*this)(at).p();
    }

    /// adds a record to the end of the snapshot list (time has to be greater than those already present)
    void add(const tsnapshot& s)
    {
        fx.push_back(s);
    }

    /// displays the history to stream \p o by intervals \p interval
    void output(std::ostream& o, ttime interval) const
    {
        o << "t,b,a,p" << std::endl;
        for(ttime t = 0; t < (fx.end()-1)->t + interval; t+= interval )
        {
            auto s = (*this)(t);
            o << t << "," << p2str(s.b) << "," << p2str(s.a) << "," << s.p() << "," << std::endl;
        }
    }
private:
    std::vector<tsnapshot> fx;

};

/// Interface to strategies, mediating the information about the current state of the market
class tmarketinfo
{

public:
    /// history of the market
    const tmarkethistory& history;
    /// current state of the order book
    const torderptrprofile& orderbook;
    /// the profile belonging to the target strategy
    const torderprofile& myprofile;
    /// id of the target strategy in the markat's list (corresponds to marketsim::torder::owner)
    unsigned myid;
    /// current time
    ttime t;
};

/// Collects state/history of the strategy's activity. Also holds information about
/// cash and stocks blocked by market.
class ttradinghistory
{
public:
    /// record about consumption
    struct tconsumptionevent
    {
        /// time of the consumption
        ttime ftime;
        /// amount consumed
        tprice famount;
    };

    /// record about a trade
    struct tradingevent
    {
        /// time of the trade
        ttime ftime;
        /// increase/descreas of the cash
        tprice fmoneydelta;
        /// increase/decrease of the (stock) inventory
        tvolume fstockdelta;
        /// other party of the trade (index in the market's list)
        unsigned partner;
    };

    /// constructor, \p name is the name of the strategy, \p wallet is the initial endowment
    ttradinghistory(const std::string& name, const twallet& wallet) :
        fname(name), fwallet(wallet), fblockedmoney(0), fblockedstocks(0) {}
    /// accessor
    void addconsumption(tprice c, ttime t)
    {
        assert(c > fwallet.money());
        fconsumption.push_back(tconsumptionevent({t,c}));
        fwallet.money() -= c;
    }
    /// accessor
    void addtrade(tprice moneydelta, tvolume stockdelta, ttime t, unsigned partner)
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

/// General base class for strategies
class tstrategy
{
    friend class tsimulation;
protected:
    /// constructor, \p name is recommended to be unique to each instance
    tstrategy(const std::string& name) : fname(name) {}
public:
    /// accessor
    const std::string& name() const { return fname;}
private:
    /// is called by marketsim::tsimulation at time, specified by the strategy at the
    /// previous event
    virtual trequest event(const tmarketinfo& ai,
                           const ttradinghistory& aresult)  = 0;
    /// is called at the end of the trading session
    virtual void atend(const ttradinghistory&) {}
    /// is called when warning arise during settlement -- by default repurts the waring to
    /// orpp::sys::log()
    virtual void warning(trequest::ewarning code, const std::string& txt)
    {
        std::ostringstream str;
        str << "Warning: '" << trequest::warninglabel(code) << "' to '"
            << name() << "': " << txt << std::endl;
//        throw str.str();
        orpp::sys::log() << str.str();
    }
private:
    std::string fname;
};

/// Results of the simulation
class tsimulationresult
{
public:
    /// history of the market
    const tmarkethistory& history;
    /// trading histories of individual strategies
    const std::vector<ttradinghistory>& tradinghistory;
};

/// Class performing the simulation. Need not be instantiated, all methods are static
/// (has to be only single one in program, however)
class tsimulation
{
    struct settlewarning
    {
        trequest::ewarning w;
        std::string text;
    };

    class torderbook
    {
    public:
        torderbook(unsigned n, tmarkethistory& history) : fbook(n), fhistory(history)
        {
        }

        std::vector<settlewarning> settle(const trequest& arequest,
                           std::vector<ttradinghistory>& profiles,
                           unsigned owner,
                           ttime t)
        {
            std::vector<settlewarning> ret;
            assert(owner < numstrategies());
            const tpreorderprofile& request = arequest.requestprofile();
            if(!request.check())
            {
                ret.push_back({trequest::ecrossedorders,""});
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
                }
                if(eraserequest.all || bfilter.size())
                {
                    profiles[owner].blockedmoney() -= fbook[owner].B.value(afilter);
                    fbook[owner].B.clear(bfilter);
                }
                sort();
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
                        es << "Only " << available << " out of " << r;
                        ret.push_back({trequest::enotenoughmoneytobuy,es.str()});
                        toexec = available;
                    }
                    if(toexec > 0)
                    {
                        profiles[owner].addtrade(-price * toexec, toexec, t,fa[j]->owner);
                        profiles[fa[j]->owner].addtrade(price * toexec, -toexec, t, owner);
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
                        es << "Only " << available << " out of " << r;
                        ret.push_back({trequest::enotenoughmoneytoput,es.str()});
                        toput = available;
                    }
                    else
                        toput = remains;
                    fbook[owner].B.add( torder(r.price,toput,t,owner) );
                    profiles[owner].blockedmoney() += r.price * toput;
                }

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
                        es << "Only " << available << " out of " << r;
                        ret.push_back({trequest::enotenoughstockstosell,es.str()});
                        toexec = available;
                    }

                    if(toexec > 0)
                    {
                        profiles[owner].addtrade(price * toexec, -toexec, t, fb[j]->owner);
                        profiles[fb[j]->owner].addtrade(-price * toexec, toexec, t, owner);
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
                        es << "Only " << available << " out of " << r;
                        ret.push_back({trequest::enotenoughstockstoput,es.str()});
                        toput = available;
                    }
                    else
                        toput = remains;
                    fbook[owner].A.add( torder(r.price,toput,t,owner) );
                    profiles[owner].blockedstocks() += toput;
                }

            }


            sort();
            fhistory.add({b(),a(),t});
            return ret;
        }

        unsigned numstrategies() const { return fbook.size(); }

        const torderprofile& profile(unsigned i) const
        {
            return fbook[i];
        }

        const tmarkethistory& history() const
        {
            return fhistory;
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

        void sort()
        {
            fshuffledowners.clear();
            fshuffledowners.resize(fbook.size());
            for(unsigned i=0; i<fbook.size(); i++)
                fshuffledowners[i] = i;
            std::shuffle(fshuffledowners.begin(),
                         fshuffledowners.end(), orpp::sys::engine() );
            makequeue<true>();
            makequeue<false>();
        }

        std::vector<unsigned> fshuffledowners;
        std::vector<torder*> fa;
        std::vector<torder*> fb;
        torderptrprofile fobprofile;
        tmarkethistory& fhistory;

        template <bool ask>
         static bool cmp(const torder *a, const torder *b)
         {
            return tsortedordercontainer<torder,ask>::cmp(*a,*b);
         }

        template <bool ask>
        void makequeue()
        {
            std::vector<torder*>& dst = ask ? fa : fb;
            dst.clear();
            for(unsigned n=0; n<fshuffledowners.size(); n++)
            {
                unsigned j = fshuffledowners[n];
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


    };

    static tsimulation& self()
    {
        static tsimulation lself;
        return lself;
    }

public:

    /// if calles with \p alogging=\c true, detailed information is sent to
    /// orpp::sys::log() during simulation
    static void setlogging(bool alogging)
    {
        self().flogging = alogging;
    }

    /// changes the simulation definition
    static void setdef(const tmarketdef& adef)
    {
        self().fdef = adef;
    }

    /// accessor
    static const tmarketdef& def()
    {
        return self().fdef;
    }

    /// \c true if logging is on
    static bool islogging()
    {
        return self().flogging;
    }

    /// adds a strategy \p s, involved in the simulation, and its initial endownment \p e,
    /// to the end if the internal list
    static void addstrategy(tstrategy& s, twallet e)
    {
        self().fstrategies.push_back(&s);
        self().fendowments.push_back(e);
    }

    /// returns the number of strategies in the internal list
    static unsigned numstrategies()
    {
        return self().fstrategies.size();
    }

    /// returns results of the last simulation
    static tsimulationresult results()
    {
        tsimulationresult ret ={ self().fhistory, self().fresults };
        return ret;
    }

    /// Run the simulation of \p timeofrun seconds of trading (the simulation is usually much shorter).
    static void run(double timeofrun)
    {
        self().dorun(timeofrun);
    }

    struct averageresult
    {
        double meanvalue;
        double standarderr;
    };


    /// Evaluates the current strategies by running \p nruns simulation of length \p timeofrun.
    /// For each strtategy it returns mean value of its profit (comparison to the "hold" strategy,
    /// i.e. doing nothing) and the standard deviation.

    static std::vector<averageresult> evaluate(double timeofrun, unsigned nruns)
    {
        std::vector<double> s(numstrategies(),0);
        std::vector<double> s2(numstrategies(),0);
        unsigned nobs = 0;
        for(unsigned i=0; i<nruns; i++)
        {
            orpp::sys::log() << i  << std::endl;
            run(timeofrun);

            for(unsigned j=0; j<numstrategies(); j++)
            {
               auto& r= results().tradinghistory[j];
               double p = results().history.p(std::numeric_limits<double>::max());
               if(isnan(p))
                   orpp::sys::log() << "nan result" << std::endl;
               else
               {
                   auto& e = self().fendowments[j];
                   double v = (r.wallet().money() - e.money())
                            + p *  (r.wallet().stocks() - e.stocks());
                   s[j]+=v;
                   s2[j]+=v*v;
                   nobs++;
               }
            }
        }
        std::vector<averageresult> ret;
        for(unsigned j=0; j<numstrategies(); j++)
        {
            double m=s[j] / nobs;
            ret.push_back({m, sqrt((s2[j] - m*m))/nobs});
        }
        return ret;
    }

private:
    void dorun(double timeofrun)
    {
         unsigned n=fendowments.size();
         fresults.clear();
         fhistory = tmarkethistory();

         assert(n==fstrategies.size());
         torderbook ob(n, fhistory);

         std::exponential_distribution<> tickdistribution(1.0 / fdef.flattency);

         double t=0.0;

         for(unsigned i=0; i<n; i++)
             fresults.push_back(ttradinghistory(fstrategies[i]->name(), fendowments[i]));

         std::vector<ttime> alarmclock(n,0);

         if(flogging)
             orpp::sys::log() << "Starting simulation with " << n << " strategies" << std::endl;
         for(;;)
         {
             std::vector<trequest> orderrequests(n);

             std::vector<unsigned> requesting;
             for(unsigned i=0; i<n; i++)
             {
                 if(alarmclock[i] < t+std::numeric_limits<double>::epsilon())
                 {
                     auto& r = fresults[i];
                     if(flogging)
                     {
                        orpp::sys::log() << "Calling stategy " <<  fstrategies[i]->name() << " at " << t << std::endl;
                        orpp::sys::log() << '\t' << "with profile " << std::endl;
                        orpp::sys::log() << '\t' << "A: " << ob.profile(i).A << std::endl;
                        orpp::sys::log() << '\t' << "B: " << ob.profile(i).B << std::endl;
                        orpp::sys::log() << '\t' << "..." << std::endl;
                     }


                     const tmarketinfo mi = { ob.history(), ob.obprofile()    ,
                                             ob.profile(i),i, t};
                     auto& request = orderrequests[i]=fstrategies[i]->event(mi,r);
                     if(flogging)
                     {
                       orpp::sys::log() << '\t' << "Requests " << std::endl;
                       orpp::sys::log() << '\t' << '\t' << "A: " << request.requestprofile().A << std::endl;
                       orpp::sys::log() << '\t' << '\t' << "B: " << request.requestprofile().B << std::endl;
//if(request.requestprofile().A.size() && request.requestprofile().A[0].price==60 && request.requestprofile().A[0].volume==1)
//    std::clog << "Here!" << std::endl;
                       if(request.eraserequest().all)
                           orpp::sys::log() << '\t' << "wanting to erase all previous orders" << std::endl;
                       else
                       {
                           orpp::sys::log() << '\t' << "Erase profiles: ";
                           orpp::sys::log() << "A:";
                           for(auto r: request.eraserequest().a)
                               orpp::sys::log() << (r ? "Y" : "N");
                           orpp::sys::log() << ", B:";
                           for(auto r: request.eraserequest().b)
                               orpp::sys::log() << (r ? "Y" : "N");
                           orpp::sys::log() << std::endl;
                       }
                       orpp::sys::log() << '\t' << "the strategy wants to consume " << request.consumption()
                                        << " and to be called after " << request.fnextupdate
                                        << " seconds." << std::endl;

                    }


                     tprice c = request.consumption();
                     if(c > 0)
                     {
                         double d = std::min(c,r.availablemoney());
                         r.wallet().money() -= d;
                         r.addconsumption(d,t);
                         // tbd warn
                     }
                     requesting.push_back(i);
                 }
             }
             std::shuffle(requesting.begin(), requesting.end(), orpp::sys::engine());
             for(unsigned i=0; i<requesting.size(); i++)
             {
                 if(flogging)
                     orpp::sys::log() << "Settling "
                                      << fstrategies[requesting[i]]->name() << std::endl;

                  std::vector<settlewarning> sr = ob.settle(orderrequests[requesting[i]],
                          fresults,
                          requesting[i],
                          t
                          );
                  for(auto r: sr)
                      fstrategies[requesting[i]]->warning(r.w, r.text);
                  alarmclock[requesting[i]] = t
                          + std::max(fdef.minupdateinterval,
                                     orderrequests[requesting[i]].nextupdate());
             }
             t+=tickdistribution(orpp::sys::engine());

             if(requesting.size() && flogging)
             {
                orpp::sys::log() << "After settlement: "  << std::endl;
                for(unsigned i=0; i<n; i++)
                {
                    orpp::sys::log() << "Strategy " << fstrategies[i]->name() << std::endl;
                    orpp::sys::log() << '\t' << "A: " << ob.profile(i).A << std::endl;
                    orpp::sys::log() << '\t' << "B: " << ob.profile(i).B << std::endl;
                    orpp::sys::log() << '\t' << "money:" << fresults[i].wallet().money()
                                     << " (blocked:" << fresults[i].blockedmoney() << ") "
                                     << "stocks:" << fresults[i].wallet().stocks()
                                     << " (blocked:" << fresults[i].blockedstocks() << ") "
                                     << std::endl;
                }
                orpp::sys::log() <<  std::endl;
             }
             if(t > timeofrun)
                 break;
         }
         for(unsigned i=0; i<n; i++)
         {
             auto p = ob.profile(i);
             auto& r = fresults[i];
             r.blockedmoney() -= p.B.value();
             r.blockedstocks() -= p.A.volume();
         }
    }

    tmarketdef fdef;
    bool flogging = false;
    std::vector<tstrategy*> fstrategies;
    std::vector<twallet> fendowments;

    std::vector<ttradinghistory> fresults;
    tmarkethistory fhistory;

};


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
                                     const ttradinghistory& history ) = 0;

    virtual trequest event(const tmarketinfo& mi,
                                  const ttradinghistory& aresult )
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
        return trequest(pp,er,next);
    }
    ttime finterval;
    bool frandom;
};



} // namespace

#endif // MARKETSIM_HPP
