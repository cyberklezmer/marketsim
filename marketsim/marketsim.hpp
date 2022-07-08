#ifndef MARKETSIM_HPP
#define MARKETSIM_HPP
#include "assert.h"
#include <vector>
#include <iostream>
#include <ostream>
#include <fstream>
#include <sstream>
#include <limits>
#include <random>
#include <atomic>
#include <algorithm>
#include <math.h>
#include "Chronos.hpp"
#include "Worker.hpp"

// the namespace encapulating all the library
namespace marketsim
{

constexpr unsigned versionmajor=1;
constexpr unsigned versionminor=98;

class error: public std::runtime_error
{
public:
   error(const std::string &what = "") : std::runtime_error(what) {}
};

class runerror: public error
{
public:
   runerror(const std::string &what = "") : error(what) {}
};

class marketsimerror: public error
{
public:
   marketsimerror(const std::string &what = "") : error(what) {}
};

/// class used to collect statistical information (TBD move to orpp)
struct statcounter
{
    double sum = 0.0;
    double sumsq = 0.0;
    unsigned num = 0;
    void add(double x) { sum += x; sumsq += x*x; num++;}
    double average() const { return sum / num; }
    double var() const { return sumsq/num - average()*average(); }
    double averagevar() const  { return var() / num; }

};


template <typename T>
class finitedistribution
{
public:
    void add(double p,const T& x)
    {
        item m = { p,x };
        fx.insert(std::upper_bound( fx.begin(), fx.end(), m , cmp),                  m                );
    }
    double lowerCVaR(double alpha) const
    {
        double sum = 0;
        double psum = 0;
        for(auto it = fx.begin(); it != fx.end(); it++)
        {
            sum += it->x * it->p;
            psum += it->p;
            if(psum >= alpha)
                break;
        }
        return sum/psum;
    }

    double mean() const { return lowerCVaR(1); }

    double meanCVaR(double lambda, double alpha) const
    {
        return lambda * lowerCVaR(alpha) + (1-lambda) * mean();
    }

    bool check() const
    {
        double psum = 0;
        for(auto it = fx.begin(); it != fx.end(); it++)
            psum += it->p;
        return fabs(psum) - 1 < 0.000000001;
    }

private:
    struct item { double p; T x; };
    static bool cmp(const item &a, const item &b)
    {
        return a.x < b.x;
    }
    std::vector<item> fx;
};

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


/// class describing the strategy's holdings
class twallet
{
public:

    /// constructs an empty wallet
    twallet() : fmoney(0), fstocks(0) {}

    /// constructs new instance with \p amoney of cash and \p astocks of stocks
    twallet(tprice amoney, tvolume astocks) : fmoney(amoney), fstocks(astocks) {}

    /// accessor
    void changemoney(tprice moneydelta)
    {
        // this should not happen if we do not put too much money into the competition
        if(static_cast<double>(fmoney)+static_cast<double>(moneydelta)
                 > std::numeric_limits<tprice>::max() / 2)
            throw marketsimerror("Wallet money overflow");
        if(fmoney + moneydelta < 0)
            throw marketsimerror("Negative money in wallet");
        fmoney += moneydelta;
    }
    /// accessor
    void changestocks(tvolume stockdelta)
    {
        // this should not happen if we do not put too much money into the competition
        if(static_cast<double>(fstocks)+static_cast<double>(stockdelta)
                 > std::numeric_limits<tvolume>::max() / 2)
            throw marketsimerror("Wallet stocksoverflow");
        if(fstocks + stockdelta < 0)
            throw marketsimerror("Negative stock in wallet");
        fstocks += stockdelta;
    }
    /// accessor
    const tprice& money() const  { return fmoney; }
    /// accessor
    const tvolume& stocks() const { return fstocks; }

    /// stream output
    void output(std::ostream& o) const
    {
        o << "m=" << fmoney << ", s=" << fstocks;
    }

    // contructs virtually unlimited wallet
    static twallet infinitewallet()
    {
        return twallet(std::numeric_limits<tprice>::max()/4,
                       std::numeric_limits<tvolume>::max()/4);
    }

    static twallet emptywallet()
    {
        return twallet(0,0);
    }


private:
    tprice fmoney;
    tvolume fstocks;
};

/// an item of marketsim::trequestresult
struct tsettleerror
{
    /// possible warnings/errors, which can be returned back when settling the request
    enum eresult { OK /* never used */ ,enotenoughmoneytoconsume,
                   ecrossedorders, einvalidorders, enotenoughmoneytobuy, enotenoughstockstosell,
                    enotenoughmoneytoput, enotenoughstockstoput,ezeroorlessprice,
                    enumresults};
    eresult w;
    std::string text;
    /// text version of the warnings, \sa eresult (TBD - not used anywhere!)
    static std::string warninglabel(eresult w)
    {
        static std::vector<std::string> lbls =
        { "OK" , "not enough money to consume",
          "crossed orders", "invalid orders", "not enough money to buy",
          "not enough stocks to sell", "not enough money to put",
          "not enough stocks to put", "0 or even less limit prace of buy" };
        return lbls[w];
    }

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

    o << "," << r.volume << "," << r.abstime << "," << r.timestamp << "," << r.owner << ")";
    return o;
}

/// Container of pointers to \p O (which has to be compatible with marketsim::tpreorder)
template <typename O>
class torderptrcontainer
{
public:
    torderptrcontainer(const std::vector<O*>* ax) : fx(ax) {}
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
            assert(r.volume >= 0);
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

    /// computes the value (sum of prices times volumes) of the orders of in the list
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

    /// computes the volume (sum of volumes) of the orders of in the list
    /// -- if \p filter is empty, then all orders are processed, otherwise only those
    /// with \c true in corresponding positions of \p filter
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

    /// output to a stream
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
    bool checkcrossed() const
    {
        tprice ap = a();
        tprice bp = b();
        return  a() > b() || (bp == khundefprice && ap==khundefprice) ||
                (bp == klundefprice && ap==klundefprice);
    }

    /// checks for undefined prices
    bool checkorders() const
    {
        for(unsigned i=0; i<A.size(); i++)
        {
            const auto& a=A[i];
            if(a.price == khundefprice)
                return false;
        }
        for(unsigned i=0; i<B.size(); i++)
        {
            const auto& b=B[i];
            if(b.price == klundefprice)
                return false;
        }
        return true;
    }



    /// output to a stream
    void output(std::ostream& o) const
    {
        o << "bids: ";
        B.output(o);
        o << " asks: ";
        A.output(o);
    }
};

/// Profile of orders
using torderprofile = tprofilebase<torderlist<false>,torderlist<true>>;

/// Profile of preorders
using tpreorderprofile = tprofilebase<tpreorderlist<false>,tpreorderlist<true>>;

/// Profile of orders, referenced by pointers
using torderptrprofile = tprofilebase<torderptrlist<false>,torderptrlist<true>>;

class tstrategy;
using tstrategyid = int;
constexpr int nostrategy = 0;

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

        /// \c true if all orders have to be eraded, regardless of \c a and \c b,
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



    /// \brief constructor
    /// \param orderrequest orders to be inserted
    /// \param eraserequest which order have to be removed from the existing profile
    /// \param consumption amount of money to be consumed
    trequest(const tpreorderprofile& orderrequest,
             const teraserequest& eraserequest = teraserequest(true),
             tprice consumption = 0) :
        fconsumption(consumption),
        forderrequest(orderrequest),
        feraserequest(eraserequest)
    {}
    /// constructs empty request
    trequest() : fconsumption(0),
        forderrequest(), feraserequest(false) {}
    /// accessor
    tprice consumption() const { return fconsumption; }
    /// accessor
    void setconsumption(tprice c) { fconsumption = c; }
    /// accessor
    void addselllimit(tprice p, tvolume v) { forderrequest.A.add(tpreorder(p,v));  }
    /// accessor
    void addbuylimit(tprice p, tvolume v) { forderrequest.B.add(tpreorder(p,v));  }
    /// accessor
    void addsellmarket(tvolume v) { forderrequest.A.add(tpreorder::marketorder<true>(v)); }
    /// accessor
    void addbuymarket(tvolume v) { forderrequest.B.add(tpreorder::marketorder<false>(v)); }
    /// accessor
    void seteraseall() { feraserequest = teraserequest(true); }

    /// accessor
    const tpreorderprofile& orderrequest() const { return forderrequest; }
    /// accessor
    const teraserequest& eraserequest() const { return feraserequest; }

    /// accessor
    void seteraserequest(const teraserequest& r) { feraserequest = r; }

    /// if true then the request would certainly have no effect
    bool empty() const
    {
        return !feraserequest.possiblyerase()
                && forderrequest.B.size() == 0
                && forderrequest.A.size() == 0
                && fconsumption == 0;
    }

    /// output to a stream
    void output(std::ostream& o) const
    {
        o << "erase ";
        if(feraserequest.all)
            o << "all";
        else if(feraserequest.b.size()==0 && feraserequest.a.size() == 0)
            o << "none";
        else
        {
            o << "(bids=";
            for(unsigned i=0; i < feraserequest.b.size(); i++)
                o << (feraserequest.b[i] ? "y" : "n");
            o << " asks=";
            for(unsigned i=0; i < feraserequest.a.size(); i++)
                     o << (feraserequest.a[i] ? "y" : "n");
            o << ")";
        }
        o << ", ";
        forderrequest.output(o);
        o << " c: " << fconsumption;
    }

private:
    tprice fconsumption;
    tpreorderprofile forderrequest;
    teraserequest feraserequest;
};

template <typename S>
class tjumpprocess
{
public:
    tjumpprocess() {}
    tjumpprocess(const S& s) : fx(1,s) {}


    template <double F(const S&)>
    double evaluate(tabstime astart, tabstime aend) const
    {
        if(!fx.size())
            return 0;

        tvolume ret = 0;
        S val(aend);
        auto it = std::lower_bound(fx.begin(),fx.end(),val,cmp);
        if(it==fx.end())
             it--;
        for(;;)
        {
            if(it->t >= astart)
                ret += F(*it);
            else
                break;
            if(it==fx.begin())
                break;
            it--;
        }
        return ret;
    }

    auto lastnogreaterthan(tabstime at) const
    {
        assert(fx.size());
        S val(at+std::numeric_limits<tabstime>::epsilon());
        auto it = std::lower_bound(fx.begin(),fx.end(),val,cmp);
        assert(it != fx.begin());
        return it - 1;
    }

    tabstime timeoflast() const
    {
        if(fx.size())
            return fx[fx.size()-1].t;
        else
            return std::numeric_limits<tabstime>::quiet_NaN();
    }


    /// returns the state of the bid and ask at a given time (\p at can be any positive number)
    const S& operator() (tabstime at) const
    {
        if(!fx.size())
            throw std::runtime_error("No records in tjumpprocess");
        auto it = lastnogreaterthan(at);
        return *it;
    }

    /// adds a record to the end of the snapshot list (time has to be greater than those already present)
    void add(const S& s)
    {
        fx.push_back(s);
    }


    static bool cmp(const S &a, const S &b)
    {
        return a.t < b.t;
    }
    const std::vector<S>& x() const { return fx; }
private:
    std::vector<S> fx;

};

/// state of the bid and ask values at a time
struct tsnapshot
{
    static constexpr double knan = std::numeric_limits<double>::quiet_NaN();

    /// bid price (may be \c klundefprice)
    tprice b;
    /// ask price (may be \c khundefprice)
    tprice a;
    /// time of the snapshot
    tabstime t;
    /// traded volume
    tvolume q;
    /// traded volume
    tvolume Bvol;
    /// traded volume
    tvolume Avol;
    /// computes the midpoint price -- if \c b or \c a are undefined, it returns \c nan
    /// (the return value should be thus checked by \c isnan() )
    double p() const
    {
        if(b==klundefprice || a==khundefprice)
            return knan;
        else
            return (a+b) / 2.0;
    }
    static double getq(const tsnapshot& s) { return s.q; }
    tsnapshot(tprice ab, tprice aa, tabstime at, tvolume aq, tvolume bvol, tvolume avol)
         : b(ab), a(aa), t(at), q(aq), Bvol(bvol), Avol(avol) {}
    tsnapshot(tabstime at) : b(klundefprice), a(khundefprice), t(at), q(0), Bvol(0), Avol(0) {}
};


/// Class serving strategies (ane later possibly for evaluations) to access the history of market
class tmarkethistory : public tjumpprocess<tsnapshot>
{
public:

    /// constructor
    tmarkethistory() : tjumpprocess<tsnapshot>(tsnapshot(0)) {}

    /// sums trades in [\p astart, \p aend)
    double sumq(tabstime astart,tabstime aend) const
    {
        return this->tjumpprocess<tsnapshot>::evaluate<tsnapshot::getq>(astart,aend);
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

    enum ewhichvaluetofind { efindb = -1, efindp, efinda };
    template <int which>
    tsnapshot lastdefined(tabstime l=0, tabstime h=std::numeric_limits<tabstime>::max()) const
    {
        assert(x().size());
        auto it=lastnogreaterthan(h);
        for(; it>=x().begin(); it--)
        {
            if(it->t < l)
                break;
            if constexpr(which==efindb)
            {
                if(it->b != klundefprice)
                    return *it;
            }
            else if constexpr(which==efinda)
            {
                if(it->a != khundefprice)
                    return *it;
            }
            else if constexpr(which==efindp)
            {
                if(it->a != khundefprice && it->b != klundefprice)
                    return *it;
            }
            else
            {
                assert(0);
            }
        }
        return tsnapshot(0);
    }

    template<int which>
    tabstime timeundefined(tabstime astart, tabstime aend) const
    {
        assert(x().size());
        auto it=lastnogreaterthan(aend);
        tabstime sum = 0;
        tabstime h = aend;
        for(;;)
        {
            tabstime l = std::max(it->t,astart);
            if constexpr(which==efindb)
            {
                if(it->b == klundefprice)
                    sum+=h-l;
            }
            else if constexpr(which==efinda)
            {
                if(it->a == khundefprice)
                    sum+=h-l;
            }
            else if constexpr(which==efindp)
            {
                if(it->a == khundefprice || it->b == klundefprice)
                    sum+=h-l;
            }
            if(it->t <= astart)
                break;
            if(it == x().begin())
                break;
            it--;
            h = l;
        }
        return sum;
    }


    double definedpin(tabstime l, tabstime h) const
    {
        return lastdefined<tmarkethistory::efindp>(l,h).p();
    }



    /// displays the history to stream \p o by intervals \p interval
    void output(std::ostream& o, tabstime interval) const
    {
        o << "t,b,a,p,q" << std::endl;
        for(tabstime t = 0; t < (x().end()-1)->t + interval; t+= interval )
        {
            auto s = (*this)(t);
            auto q = t==0 ? 0 : sumq(t-interval+std::numeric_limits<tabstime>::epsilon(),t);
            o << t << "," << p2str(s.b) << "," << p2str(s.a) << "," << s.p() << ","
              << q << std::endl;
        }
    }
};


/// record about a demand/supply
struct tdsevent
{
    /// time of the trade
    tabstime t;
    /// money from d/s
    tprice demand;
    /// increase/decrease of the (stock) inventory
    tvolume supply;
    tdsevent(tabstime at): t(at),demand(0), supply(0) {}
    tdsevent(tabstime at, tprice ademand, tvolume asupply )
        : t(at),demand(ademand), supply(asupply) {}
    static double getdemand(const tdsevent& e) { return e.demand; }
    static double getsupply(const tdsevent& e) { return e.supply; }
};


/// Stores the state of the wallet and the history of the strategy's activity. Also holds information about
/// cash and stocks blocked by market.
class tstrategyinfo
{
    friend class tmarket;
public:
    /// record about consumption
    struct tconsumptionevent
    {
        /// time of the consumption
        tabstime t;
        /// amount consumed
        tprice famount;
        tconsumptionevent(tabstime at) : t(at), famount(0) {}
        tconsumptionevent(tabstime at,tprice amount): t(at), famount(amount) {}
        static double getc(const tconsumptionevent& e) { return e.famount; }
    };

    /// record about a trade
    struct tradingevent
    {
        /// time of the trade
        tabstime t;
        /// increase/descreas of the cash
        tprice moneydelta;
        /// increase/decrease of the (stock) inventory
        tvolume stockdelta;
        /// other party of the trade (index in the market's list)
        unsigned partner;
        tradingevent(tabstime at) : t(at), moneydelta(0), stockdelta(0),
            partner(std::numeric_limits<unsigned>::max()) {}
        tradingevent(tabstime at, tprice amoneydelta, tvolume astockdelta, unsigned apartner)
            : t(at), moneydelta(amoneydelta), stockdelta(astockdelta), partner(apartner)
             {}
        static double getstockpurchase(const tradingevent& e) { return std::max(0,e.stockdelta); }
        static double getstockselling(const tradingevent& e) { return std::max(0,-e.stockdelta); }
    };



    /// constructor, \p endowment is the initial state of the wallet,
    /// \p id is the (unique) identification of the strategy,
    /// \p name is a (possibly non-unique) text description of the strategy
    tstrategyinfo(const twallet& endowment, tstrategyid id, const std::string& name) :
        fid(id), fname(name), fwallet(endowment), fblockedmoney(0), fblockedstocks(0) {}
    /// copy constructor
    tstrategyinfo(const tstrategyinfo& src) :
        fid(src.fid), fname(src.fname), fwallet(src.fwallet), fblockedmoney(src.fblockedmoney),
        fblockedstocks(src.fblockedstocks),
        fconsumption(src.fconsumption), ftrading(src.ftrading),fds(src.fds), fsublog(src.fsublog.str()),
        fendedbyexception(src.fendedbyexception), ferrmsg(src.ferrmsg),
        foverrun(src.foverrun)
    {}
    /// accessor
    void addconsumption(tprice c, tabstime t)
    {
        assert(c <= fwallet.money());
        fconsumption.add(tconsumptionevent(t,c));
        fwallet.changemoney(-c);
    }
    /// accessor
    void addtrade(tprice moneydelta, tvolume stockdelta, tabstime t, unsigned partner)
    {
        assert(fwallet.money() + moneydelta >= 0);
        assert(fwallet.stocks() + stockdelta >= 0);
        ftrading.add(tradingevent(t, moneydelta, stockdelta, partner));
        fwallet.changemoney(moneydelta);
        fwallet.changestocks(stockdelta);
    }
    /// accessor
    void addds(tprice demand, tvolume supply, tabstime t)
    {
        fds.add(tdsevent(t, demand, supply));
        fwallet.changemoney(demand);
        fwallet.changestocks(supply);
    }
    /// accessor
    const twallet& wallet() const { return fwallet; }
    /// accessor
    twallet& wallet() { return fwallet; }
    /// accessor
    const tjumpprocess<tconsumptionevent>& consumption() const { return fconsumption; }

    /// computes the sum of all consumptions
    tprice totalconsumption() const
    {
        return fconsumption.evaluate<tconsumptionevent::getc>(0,std::numeric_limits<tabstime>::max());
/*        tprice c = 0;
        for(unsigned k=0; k<fconsumption.size(); k++)
            c += fconsumption[k].famount;
        return c;*/
    }

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
    const tjumpprocess<tradingevent>& tradinghistory() const { return ftrading; }
    /// returns ds history
    const tjumpprocess<tdsevent>& dshistory() const { return fds; }
//    /// returns the name of the strategy
    tstrategyid strategyid() const { return fid; }

    /// indicates degree of detail of the output, \see output
    enum eoutputlevel { enone, ewallet, etrades, enumoutputlevels};

    /// lists the trading history and results to stream \p o at detail level \p level
    void output( std::ostream& o, eoutputlevel level) const
    {

        if(level >= ewallet)
        {
            o << "Money: " << wallet().money()
                             << ", stocks: " << wallet().stocks()
                             << std::endl;
        }
        if(level >= etrades)
        {
            o << "Trades:"  << std::endl;
            for(auto tr: tradinghistory().x())
                o << tr.t << "s: " << tr.moneydelta << "$, "
                  << tr.stockdelta << " with " << tr.partner << std::endl;
        }
    }

private:
    /// tells the class that the strategy run has been prematurely ended by exception with
    /// error text \p s (used by marketsim::tmarket)
    void reportexception(const std::string& s)
    {
        ferrmsg = s;
        fendedbyexception = true;
    }

    /// tells the class that the strategy exceeded the lemit marketsim::tmarketdef::tickstowait upon
    /// the end of the simulation (used by marketsim::tmarket)
    void reportoverrun()
    {
        foverrun = true;
    }

public:

    /// accessor, \see reportexception
    bool isendedbyexception() const
    {
        return fendedbyexception;
    }

    /// accessor, \see reportexception
    const std::string& errmsg() const
    {
        return ferrmsg;
    }

    /// accessor, \see reportoverrun
    bool isoverrun() const
    {
        return foverrun;
    }

    /// accessor
    const std::string& name() const
    {
        return fname;
    }

    void addcomptime(tabstime t) {fcomptimes.add(t);}
    const statcounter& comptimes() const { return fcomptimes; }
protected:
    tstrategyid fid;
    std::string fname;
    twallet fwallet;
    tprice fblockedmoney;
    tvolume fblockedstocks;
    tjumpprocess<tconsumptionevent> fconsumption;
    tjumpprocess<tradingevent> ftrading;
    tjumpprocess<tdsevent> fds;
    statcounter fcomptimes;

    std::ostringstream fsublog;
    bool fendedbyexception = false;
    std::string ferrmsg;
    bool foverrun = false;
};

/// result of setlement
struct trequestresult
{
    /// volume traded
    tvolume q = 0;
    /// errors encountered
    std::vector<tsettleerror> errs;
};


class tmarket;
class tmarketinfo;
class tmarketdata;

struct tdsrecord
{
    tvolume d;
    tvolume s;
};

class tdsbase
{
protected:
    friend class tmarket;
    friend class tmarketdata;
    tmarket *fmarket;
protected:
    std::default_random_engine& engine();

    /// returns an uniform random variable
    double uniform();

public:
    tdsbase() : fmarket(0) {}

    virtual ~tdsbase() {}
    virtual tdsrecord delta(tabstime, const tmarketdata&)=0;
};

class tnodemandsupply : public tdsbase
{
    friend class tmarket;
    virtual tdsrecord delta(tabstime, const tmarketdata&) { return {0,0}; }
};


//class tdscreator
//{
//public:
//    virtual typename selectstragegybase<chronos>::basetype* create()
//};

/// General base class for strategies
class tstrategy: private chronos::Worker
{
private:
    template<typename S, bool chronos, bool nowarmup>
    friend class competitor;
    friend class tmarket;
    friend class tmarketdata;
    friend class teventdrivenstrategy;

    /// implements a mandatory method of Chronos::Worker
    virtual void main() override;

    /// assigns an unique (within run of the application) id to a strategy
    static tstrategyid newid()
    {
        static int currid = 1;
        return currid++;
    }
protected:

    /// constructor, \p name is recommended to be unique to each instance
    tstrategy(tabstime warmingtime = 0) : fid(newid()), fmarket(0), fwarmingtime(warmingtime)
    {
    }
//    ~tstrategy() {}
    virtual bool acceptsdemand() const  { return false; }
    virtual bool acceptssupply() const  { return false; }


    /// need to be implemented by descenants
    virtual void trade(twallet /* endowment */) = 0;



private:
    // With RT it is directly called by marketsim::tstrategy::request,
    // with ED, it is called by marketsim::teventstragegy::trade d
    template <bool chronos>
    trequestresult internalrequest(const trequest& request, tabstime t=0);

    template <bool chronos = true>
    tmarketinfo internalgetinfo();
protected:

    /// Sends a request \p request to the market (RT interface).
    trequestresult request(const trequest& request)
    {
        return internalrequest<true>(request);
    }

    /// Returns a state of the market as of the time the method is called (RT interface).
    tmarketinfo getinfo();

    /// Get curreint time (RT interface).
    tabstime abstime();

    /// Pauses own excecution for \p t units of time (wannabe seconds) (RT interface).
    void sleepfor(tabstime t);

    /// Pauses own excecution until time \p t (RT interface)
    void sleepuntil(tabstime t);

    /// Tests whether trading stopped (RT interface). Should be called within marketsim::tstrategy::trade
    /// regularly in the order of seconds (if the execution does not stop after
    /// marketsim::tmarketdef::timeowaitafterend "seconds", then the strategy is put to a
    /// "garbage" and possibly block a computer's resources while the competition possibly
    /// continues; thus such strategy can be possibly manually blacklisted by the one who implements
    /// the competition).
    bool endoftrading();

protected:
    const tmarket* market() const { return fmarket; }
    void possiblylog(const std::string&
                 shortmsg, const std::string& longmsg = "");

    std::default_random_engine& engine();

    /// returns an uniform random variable
    double uniform();


private:
    tstrategyid fid;
    /// is set by (friend) class tmarket
    twallet fendowment;
    tmarket* fmarket;
    tabstime fwarmingtime;

};


template <bool chronos>
struct selectstragegybase;

class teventdrivenstrategy;

template<>
struct selectstragegybase<false>
{
    using basetype = teventdrivenstrategy;
};

template<>
struct selectstragegybase<true>
{
    using basetype = tstrategy;
};

/// Base class for various "competitor" classes, serving solely for constructing strategies
/// "on demand" of the simulator.
template <bool chronos>
class competitorbase
{
public:
    /// Intended to create a pointer to a strategy.
    /// In dependence on \p chronos template parameter, the pointer is of type
    /// marketsim::tstrategy (when chronos==true) or marketsim::teventdrivenstrategy.
    virtual typename selectstragegybase<chronos>::basetype* create() = 0;
    /// Intended to provide a string identification of a strategy
    virtual std::string name() const = 0;
    virtual bool startswithoutwarmup() const = 0;

};


/// Actual competitor strategy constructing a \p cronos version of strategy \p S, which
/// is or is not \p builtin. \p S have to dispose of a default constructor.
/// (I wonder now why this definition is two-stage one,
/// maybe for some other versions of competitor class.s...).
template<typename S, bool chronos = true, bool nowarmup = false>
class competitor : public competitorbase<chronos>
{
    const std::string fname;
public:
    /// constructor
    competitor<S,chronos,nowarmup>(const std::string& aname = typeid(S).name())
        : fname(aname) {}

    /// actual \c create method (why it has a pure parent?)
    virtual typename selectstragegybase<chronos>::basetype* create()
    {
        S* s;
        s= new S;
        return s;
    }
    /// accessor
    virtual std::string name() const { return fname; }
    virtual bool startswithoutwarmup() const { return nowarmup; }
};


/// Results of the simulation
class tmarketresult
{
public:
    /// history of the market
    const tmarkethistory& history;
    /// infos of individual strategies
    const std::vector<tstrategyinfo>& strategyinfos;
};

/// if logging is set, an instance of this class determines what to log
struct tloggingfilter
{
    tloggingfilter()
    {
        fsettle = false;
        fsleep = false;
        ftick = false;
        fgetinfo = false;
        frequest = false;
        fabstime = false;
        fmarketdata = false;
        fds = false;
        fprotocol = false;
    }
    bool frequest;
    bool fsettle;
    bool fsleep;
    bool fgetinfo;
    bool fmarketdata;
    bool ftick;
    bool fabstime;
    bool fds;
    bool fprotocol;
};


/// struct defining market behavior
struct tmarketdef
{
private:
    /// custom error class
    class calibrateerror: public error
    {
    public:
       calibrateerror(const std::string &what = "") : error(what) {}
    };

    /// used for calibration only: puts (quite nonsensial) orders as frequently as possible
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

    /// estimated duration of one Chronos given that \p numstrategies are competing.
    /// and the market parameters (save tmarketdef::chronosduration) are as \p def
    /// the template parameter \p logging should be set analogously to \c logging parameter
    /// used in marketsim::tmarket::run.
    template <bool logging>
    static int findduration(unsigned nstrategies, const tmarketdef& def, std::ostream& log = std::clog);
public:

    /// calibrates \p def, namely sets  marketsim::tmarketdef::chronosduration
    /// do that it is minimal but hign enogh for the simulator to server one
    /// request per strategy within a single Chronos tick. \p logging
    /// has to be saved the same way as in further simulations.
    template <bool logging = false>
    void calibrate(unsigned ncompetitors, std::ostream& log = std::clog)
    {
        log << "Calibrating for " << ncompetitors << " competitors." << std::endl;
        chronosduration = chronos::app_duration(findduration<logging>(ncompetitors,*this,log));
        log << "ticktime() = " << ticktime() << std::endl;
    }

    /// \p chronos duration (number of chronos::app_duration units one chronos tick takes)
    /// Best is to use marketsim::calibrate to set it in run-time.
    chronos::app_duration chronosduration = chronos::app_duration(100000);

    /// magnitude of "noise" added to the waiting times given ED
    double epsilon = 0.0000001;

    /// time the simulator waits for marketsim::tstrategy::trade to finish after
    /// time horizon was reached
    tabstime timeowaitafterend = 10;

    tabstime demandupdateperiod = 0.1;

    tabstime warmuptime = 1;

    /// if \c false and logging is used then the logging is done to std::ostringstream's first and then written,
    /// if true and logging is used, then the log entries are written immediately on logging,
    /// in which case, however, only marketsim::tmarket entries are logged, not the strategies'ones,
    /// for thread safety.
    bool directlogging = false;

    /// logging filter
    tloggingfilter loggingfilter = tloggingfilter();

    /// strategies logging their custom entries (by their ordering submitted to run)
    std::vector<unsigned> loggedstrategies;

    /// numbor of snapshots in protocol (loggingfilter.fprotocol = true)
    unsigned numsnapshotsinprotocol = 10;

    /// returns constant converting a duration of the chrnonos tick to time in seconds
    double ticktime() const
    {
        double tl = static_cast<double>(chronos::app_duration::period::num)
            /static_cast<double>(chronos::app_duration::period::den);
        return chronosduration.count() * tl;
    }
};




/// a collection of all the pending orders on the market
class torderbook
{
public:
    /// constructor. \p n is the number of strategies
    torderbook(unsigned n) : fbook(n)
    {
    }

    /// used when creating shapshots
    torderbook duplicate() const
    {
        auto ret(*this);
        ret.sort();
        return ret;
    }

    /// orders that marketsim::torderbook::fa (if \p ask is true) or marketsim::torderbook::fb
    /// according to FIFO priority (probably could be implemented more efficiently)
    template <bool ask>
    void makequeue()
    {
         std::vector<torder*>& dst = ask ? fa : fb;
         dst.clear();
         /// here we newly make pointers to all the orders
         for(unsigned j=0; j<numagents(); j++)
         {
             torderlist<ask>* src;
             if constexpr(ask)
                 src = &fbook[j].A;
             else
                 src = &fbook[j].B;
             src->removezeros();
             unsigned s = src->size();
             for(unsigned i=0; i<s; i++)
                 dst.push_back(&((*src)[i]));
         }
         /// and sort them
         std::stable_sort(dst.begin(), dst.end(), cmp<ask>);
    }

    void sort()
    {
        makequeue<true>();
        makequeue<false>();
    }

    bool consistencycheck(std::vector<tstrategyinfo>& profiles)
    {
        bool error = false;
        for(unsigned owner=0; owner<fbook.size(); owner++)
        {
            for(unsigned i=0; i<fbook[owner].A.size(); i++)
            {
                if(fbook[owner].A[i].price < 0)
                {
                    std::cerr << "owner" << owner << " A price < 0" << std::endl;
                    error = true;
                }
                if(fbook[owner].A[i].volume < 0)
                {
                    std::cerr << "A volume < 0"<< std::endl;
                    error = true;
                }
            }
            for(unsigned i=0; i<fbook[owner].B.size(); i++)
            {
                if(fbook[owner].B[i].price < 0)
                {
                    std::cerr <<  "owner" << owner << " B price < 0" << std::endl;
                    error = true;
                }
                if(fbook[owner].B[i].volume < 0)
                {
                    std::cerr <<  "owner" << owner << " B volume < 0" << std::endl;
                    error = true;
                }
            }

            tprice bm1 = fbook[owner].B.value();
            tprice bm2 = profiles[owner].blockedmoney();
            if(bm1 != bm2)
            {
                std::cerr << "value of B " << bm1 << ", blocked: " << bm2 << std::endl;
                error = true;
            }
            tvolume bv1 = fbook[owner].A.volume();
            tvolume bv2 = profiles[owner].blockedstocks();
            if(bv1 != bv2)
            {
                std::cerr << "volume of A " << bv1 << ", blocked: " << bv2 << std::endl;
                error = true;
            }
        }
        return !error;
    }

    /// settles the request \p arequest with the existing orders, updates \p profiles. Here,
    /// \p owner is the index of the strategy issuing \p arequest, \ts is a current timestamp
    /// and \p at is the current absolute time.
    /// First the consumption request is fulfilled. If there is not enough money to fulfill it,
    /// maximum possible is done and warning is sent via the return value. Then the settlement
    /// is done according to FIFO altorithm. If there is not enough mony or stocks to buy, sell,
    /// respectively, the request (for a single particular order) is not fulfilled and a
    /// warning is issued. Same with insufficiency of money/stocks needed to be blocked when
    /// limit order is sissued.
    trequestresult settle(
                       const trequest& arequest,
                       std::vector<tstrategyinfo>& profiles,
                       unsigned owner,
                       ttimestamp ts,
                       tabstime at)
    {
        assert(consistencycheck(profiles));
        trequestresult ret;
        assert(owner < numstrategies());
        auto am = profiles[owner].availablemoney();
        auto c = arequest.consumption();
        assert(c>=0);
        if(c > am)
        {
            std::ostringstream es;
            es << "Not enough money to consume " << c << " stocks. Only "
               << am << " available";
            ret.errs.push_back({tsettleerror::enotenoughmoneytoconsume,es.str()});
            c=am;
        }
        if(c > 0)
        {
            auto& r = profiles[owner];
            r.addconsumption(c,at);
        }
        tpreorderprofile request = arequest.orderrequest();
        if(!request.checkcrossed())
        {
            ret.errs.push_back({tsettleerror::ecrossedorders,"Crossed orders."});
            return ret;
        }
        if(!request.checkorders())
        {
            ret.errs.push_back({tsettleerror::einvalidorders,"Invalid orders."});
            return ret;
        }
        const trequest::teraserequest& eraserequest = arequest.eraserequest();

        if(eraserequest.possiblyerase())
        {
            std::vector<bool> afilter, bfilter;
            auto& A = fbook[owner].A;
            auto& B = fbook[owner].B;

            if(!eraserequest.all)
            {
                afilter = eraserequest.a;
                bfilter = eraserequest.b;
            }
            else
            {
                afilter = std::vector<bool>(A.size(),true);
                bfilter = std::vector<bool>(B.size(),true);
            }
            if(afilter.size())
            {
                for(unsigned i=0; i<A.size();i++)
                    if(i<afilter.size() && afilter[i])
                    {
                        for(unsigned j=0; j<request.A.size(); j++)
                        {
                            if(request.A[j].price == A[i].price )
                            {
                                if(request.A[j].volume >= A[i].volume)
                                {
                                    afilter[i] = false;
                                    request.A[j].volume -= A[i].volume;
                                    assert(request.A[j].volume>=0);
                                }
                                break;
                            }
                        }
                    }
                profiles[owner].blockedstocks() -=
                   fbook[owner].A.volume(afilter);
                A.clear(afilter);
                makequeue<true>();
            }
            if(bfilter.size())
            {
                for(unsigned i=0; i<B.size();i++)
                    if(i<bfilter.size() && bfilter[i])
                    {
                        for(unsigned j=0; j<request.B.size(); j++)
                        {
                            if(request.B[j].price == B[i].price )
                            {
                                if(request.B[j].volume >= B[i].volume)
                                {
                                    bfilter[i] = false;
                                    request.B[j].volume -= B[i].volume;
                                    assert(request.B[j].volume>=0);
                                }
                                break;
                            }
                        }
                    }
                profiles[owner].blockedmoney() -=fbook[owner].B.value(bfilter);
                fbook[owner].B.clear(bfilter);
                makequeue<false>();
            }
        }

//            if(request.b() >= fbook[owner].a()
//                || request.a() <= fbook[owner].b())
//                return;

        unsigned j = 0;
        for(unsigned i = 0; i<request.B.size(); i++)
            if(request.B[i].volume > 0)
            {
                tpreorder r = request.B[i];
                tvolume remains = r.volume;
                if(r.price <= klundefprice)
                   ret.errs.push_back({tsettleerror::ezeroorlessprice,"Zero or less price for buy order"});
                else
                {
                    assert(remains >= 0);
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
                            ret.errs.push_back({tsettleerror::enotenoughmoneytobuy,es.str()});
                            toexec = available;
                        }
                        if(toexec > 0)
                        {
                            profiles[owner].addtrade(-price * toexec, toexec, at,fa[j]->owner);
                            profiles[fa[j]->owner].addtrade(price * toexec, -toexec, at, owner);
                            profiles[fa[j]->owner].blockedstocks() -= toexec;
                            fa[j]->volume -= toexec;
                            remains -= toexec;
                            ret.q += toexec;
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
                            es << "Not enough money to back bid " << r << ", only "
                               << available << " could be put.";
                            ret.errs.push_back({tsettleerror::enotenoughmoneytoput,es.str()});
                            toput = available;
                        }
                        else
                            toput = remains;
                        fbook[owner].B.add( torder(r.price,toput,ts,at,owner) );
                        profiles[owner].blockedmoney() += r.price * toput;
                    }
                    sort();
                }
            }

        j = 0;
        for(unsigned i = 0; i<request.A.size(); i++)
            if(request.A[i].volume>0)
            {
                tpreorder r = request.A[i];
                tvolume remains = r.volume;
                assert(remains >= 0);

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
                        ret.errs.push_back({tsettleerror::enotenoughstockstosell,es.str()});
                        toexec = available;
                    }

                    if(toexec > 0)
                    {
                        profiles[owner].addtrade(price * toexec, -toexec, at, fb[j]->owner);
                        profiles[fb[j]->owner].addtrade(-price * toexec, toexec, at, owner);
                        profiles[fb[j]->owner].blockedmoney() -= fb[j]->price * toexec;
                        fb[j]->volume -= toexec;
                        remains -= toexec;
                        ret.q += toexec;
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
                        ret.errs.push_back({tsettleerror::enotenoughstockstoput,es.str()});
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
        assert(consistencycheck(profiles));
        return ret;
    }

    /// accessor to the number of strategies
    unsigned numstrategies() const { return fbook.size(); }

    /// returns the profile of pending orders of strategy \p i
    const torderprofile& profile(unsigned i) const
    {
        return fbook[i];
    }

    /// returns the best ask (assumes marketsim::torderbook::fa is ordered)
    tprice a() const
    {
        if(fa.size())
            return fa[0]->price;
        else
            return khundefprice;
    }

    /// returns the best bid (assumes marketsim::torderbook::fb is ordered)
    tprice b() const
    {
        if(fb.size())
            return fb[0]->price;
        else
            return klundefprice;
    }

    /// returns ptrs to the orderbook (TBD cancel)
    torderptrprofile obprofile() const
    {
        torderptrprofile ret;
        torderptrlist<false> bl(&fb);
        torderptrlist<true> al(&fa);
        ret.setlists(bl,al);
        return ret;
    }

    unsigned numagents() const
    {
        return fbook.size();
    }


    /// used for sorting the ptrs
    template <bool ask>
    static bool cmp(const torder *a, const torder *b)
    {
        return tsortedordercontainer<torder,ask>::cmp(*a,*b);
    }

    /// here, the pending orders are actually stored
    std::vector<torderprofile> fbook;

    /// ordered list of sell orders (the class marketsim::torderbook is responsitble
    /// for keeping them sorted). TBD make it private
    std::vector<torder*> fa;

    /// ordered list of buy orders, \see marketsim::torderbook::fb.
    std::vector<torder*> fb;

};

/// this class exclusively holds the state and the history of the market
class tmarketdata
{
    /// used by constructor (not to raise exceptions in its body)
    static std::vector<tstrategyinfo> makeinfos(const std::vector<twallet>& endowments,
                                              const std::vector<tstrategy*>& strategies,
                                              const std::vector<std::string>& names     )
    {
        std::vector<tstrategyinfo> ret;
        for(unsigned i=0; i<endowments.size(); i++)
            ret.push_back(tstrategyinfo(endowments[i],strategies[i]->fid,names[i]));
        return ret;
    }
public:
    /// constructor
    /// \p endowments - endowments of individual strategies,
    /// \p strategies - pointers to strategies, have to keep existing until marketsim::tmarket::run
    /// finishes
    /// \p names - string idenntification of strategies (uniqueness not necessary, but recommended)
    tmarketdata(const std::vector<twallet>& endowments,
                const std::vector<tstrategy*>& strategies,
                tdsbase* ds,
                const std::vector<std::string>& names)
       :  fstrategyinfos(makeinfos(endowments,strategies,names)),
          forderbook(endowments.size()),
          ftimestamp(0),
          fds(ds),
          fstartclocktime(::clock())
    {}

    /// copy constructor, does not copy llog
    tmarketdata(const tmarketdata& src) :
        fstrategyinfos(src.fstrategyinfos),
        fhistory(src.fhistory),
        forderbook(src.forderbook.duplicate()),
        ftimestamp(src.ftimestamp),
        fmarketsublog(src.fmarketsublog.str()),
        fds(src.fds),
        fstartclocktime(src.fstartclocktime),
        fextraduration(src.fextraduration)
    {
    }
    /// infos of strategies
    std::vector<tstrategyinfo> fstrategyinfos;
    /// demand and supply creator
    std::shared_ptr<tdsbase> fds;
    /// history
    tmarkethistory fhistory;
    /// order book
    torderbook forderbook;



    /// current timestamp (unique integer used for determining priority of the orders)
    ttimestamp ftimestamp;
    /// system time in seconds at the start of simulations (trimmed to seconds)
    clock_t fstartclocktime;
    /// stream for log entries originated by marketsim::tmarket
    std::ostringstream fmarketsublog;
    /// collects remaining time in ticks (used by marketsim::calibrate)
    statcounter fextraduration;


    ///
    tprice a() const { return forderbook.obprofile().A.minprice(); }
    /// returns current bid
    tprice b() const { return forderbook.obprofile().B.maxprice(); }

    /// returns last defined ask
    tprice lastdefineda() const
    {
        auto canda= a();
        if(canda==khundefprice)
            return fhistory.lastdefined<tmarkethistory::efinda>().a;
        else
            return canda;
    }
    /// returns last defined bid
    tprice lastdefinedb() const
    {
        auto candb= b();
        if(candb==klundefprice)
        {
//if(fhistory.lastdefined<tmarkethistory::efindb>().b==klundefprice)
//    std::cout << "ldb=undef" << std::endl;
            return fhistory.lastdefined<tmarkethistory::efindb>().b;
        }
        else
            return candb;
    }

    /// returns last defined price
    double lastdefinedp() const
    {
        if(b()==klundefprice || a()==khundefprice)
        {
            auto s = fhistory.lastdefined<tmarkethistory::efindp>();
            return s.p();
        }
        else
            return (b() + a()) / 2.0;
    }

    /// number of strategies
    unsigned n() const { return fstrategyinfos.size(); }


    /// puts a protocol to the log stream. Consists of:
    ///
    /// List of stragegies, for each of which it shows \c c - consumption, \c m - money,
    /// \c n = stocks, \c #events - number of calls to marketsim::teventstrategy::event
    /// \c ave_comp_time = average time spent in the event method
    ///
    /// Time shapshots (their # is given by \p nsnaps): \c b - current bid,
    /// \c %_undef - percent of the interval the bid was undefined
    /// \c B_volume - number of stocks in the bid order book at the time of snapshot
    /// \c a, \c %_undef, \c A_volume - same for asks
    /// \c q - number of trades within interval
    ///
    /// Consumption of individual strategies within intervals
    ///
    /// Purchases/Selling of individual strategies
    ///
    /// Demand/Supply passed to the strategies
    ///
    void protocol(std::ostream& o, tabstime T, unsigned nsnaps = 10) const
    {
        o << std::endl << "Protocol of a market simulation"
          << std::endl << std::endl;
        o << "strategies:";

        for(unsigned i=0; i<fstrategyinfos.size(); i++)
            o << "," << fstrategyinfos[i].name();
        o << std::endl;
        o << "c:";
        for(unsigned i=0; i<fstrategyinfos.size(); i++)
            o << "," << fstrategyinfos[i].totalconsumption();
        o << std::endl;
        o << "m:";
        for(unsigned i=0; i<fstrategyinfos.size(); i++)
            o << "," << fstrategyinfos[i].wallet().money();
        o << std::endl;
        o << "n:";
        for(unsigned i=0; i<fstrategyinfos.size(); i++)
            o << "," << fstrategyinfos[i].wallet().stocks();
        o << std::endl;
        o << "# events:";
        for(unsigned i=0; i<fstrategyinfos.size(); i++)
            o << "," << fstrategyinfos[i].comptimes().num;
        o << std::endl;
        o << "ave comp time:";
        for(unsigned i=0; i<fstrategyinfos.size(); i++)
            o << "," << fstrategyinfos[i].comptimes().average();
        o << std::endl;
        o << std::endl;

        o << "time snapshots:";

        tabstime dt = T/nsnaps;
        if(isnan(T))
            o << ",no records in market history" << std::endl;
        else
        {
            for(unsigned i=1; i<=nsnaps; i++)
                o << "," << i * dt;
            o << std::endl;
            o << "b:";
            for(unsigned i=1; i<=nsnaps; i++)
                o << "," << p2str(fhistory(i * dt).b);
            o << std::endl;



//            o << "last def in int:";
//            for(unsigned i=1; i<=nsnaps; i++)
//            {
//                o << ",";
//                tprice lb = this->fhistory.lastdefined<tmarkethistory::efindb>((i-1)*dt,i * dt).b;
//                if(lb != klundefprice)
//                    o << lb;
//            }
//            o << std::endl;
            o << "% undef:";
            for(unsigned i=1; i<=nsnaps; i++)
                o << "," << 100 * this->fhistory.timeundefined<tmarkethistory::efindb>((i-1)*dt,i * dt) / dt;
            o << std::endl;

            o << "B volume:";
            for(unsigned i=1; i<=nsnaps; i++)
                o << "," << fhistory(i * dt).Bvol;
            o << std::endl;

            o << "a:";
            for(unsigned i=1; i<=nsnaps; i++)
                o << "," << p2str(fhistory(i * dt).a);
            o << std::endl;
            o << "% undef:";
            for(unsigned i=1; i<=nsnaps; i++)
                o << "," << 100 * this->fhistory.timeundefined<tmarkethistory::efinda>((i-1)*dt,i * dt)/dt;
            o << std::endl;

            o << "A volume:";
            for(unsigned i=1; i<=nsnaps; i++)
                o << "," << fhistory(i * dt).Avol;
            o << std::endl;


            o << "q:";
            for(unsigned i=1; i<=nsnaps; i++)
                o << "," << fhistory.sumq((i-1)*dt,i * dt);
            o << std::endl;
            o << std::endl;

            o << "Consumption" << std::endl;
            for(unsigned i=0; i<fstrategyinfos.size(); i++)
            {
                o << fstrategyinfos[i].name();
                for(unsigned j=1; j<=nsnaps; j++)
                    o << "," << fstrategyinfos[i].consumption().evaluate
                           <tstrategyinfo::tconsumptionevent::getc>((j-1)*dt,j * dt);
                o << std::endl;
            }

            o << std::endl;


            o << "Purchases" << std::endl;
            for(unsigned i=0; i<fstrategyinfos.size(); i++)
            {
                o << fstrategyinfos[i].name();
                for(unsigned j=1; j<=nsnaps; j++)
                    o << "," << fstrategyinfos[i].tradinghistory().evaluate
                           <tstrategyinfo::tradingevent::getstockpurchase>((j-1)*dt,j * dt);
                o << std::endl;
            }

            o << std::endl;

            o << "Selling" << std::endl;
            for(unsigned i=0; i<fstrategyinfos.size(); i++)
            {
                o << fstrategyinfos[i].name();
                for(unsigned j=1; j<=nsnaps; j++)
                    o << "," << fstrategyinfos[i].tradinghistory().evaluate
                           <tstrategyinfo::tradingevent::getstockselling>((j-1)*dt,j * dt);
                o << std::endl;
            }

            o << std::endl;

            o << "Demand" << std::endl;
            for(unsigned i=0; i<fstrategyinfos.size(); i++)
            {
                o << fstrategyinfos[i].name();
                for(unsigned j=1; j<=nsnaps; j++)
                    o << "," << fstrategyinfos[i].dshistory().evaluate
                           <tdsevent::getdemand>((j-1)*dt,j * dt);
                o << std::endl;
            }

            o << std::endl;

            o << "Supply" << std::endl;
            for(unsigned i=0; i<fstrategyinfos.size(); i++)
            {
                o << fstrategyinfos[i].name();
                for(unsigned j=1; j<=nsnaps; j++)
                    o << "," << fstrategyinfos[i].dshistory().evaluate
                           <tdsevent::getsupply>((j-1)*dt,j * dt);
                o << std::endl;
            }
            o << std::endl;
        }
    }
};

/// accessor of a particular strategy to marketsim::tmarketdata (the strategy should not see
/// all the data, especially not the situation of the competing strategy; therefore,
/// marketsim::tmarketdata cannot be passed to them)
/// (TBD maybe get rid of orderprofile - accessing A ane B directly is neough)
class tmarketinfo
{

public:
    tmarketinfo(const std::shared_ptr<tmarketdata> data, unsigned myindex):
        fdata(data),
        fmyindex(myindex)
    {
    }
    /// tbd

    /// history of the market
    const tmarkethistory& history() const { return fdata->fhistory; }
    /// current state of the order book
    torderptrprofile orderbook() const { return fdata->forderbook.obprofile(); }
    /// the profile belonging to the target strategy
    const torderprofile& myorderprofile() const { return fdata->forderbook.profile(fmyindex); }

    /// accessor to onn info
    const tstrategyinfo& myinfo() const { return fdata->fstrategyinfos[fmyindex];}

    /// accessor to onn index
    unsigned myindex() const { return fmyindex; }

    /// accessor to onw wallet
    const twallet& mywallet() const { return myinfo().wallet(); }

    /// returns current ask
    tprice a() const { return fdata->a(); }
    /// returns current bid
    tprice b() const { return fdata->b(); }

    tprice lastdefinedb() const { return fdata->lastdefinedb(); }

    tprice lastdefineda() const { return fdata->lastdefineda(); }

    double lastdefinedp() const { return fdata->lastdefinedp(); }

    /// returns current the-others-ask (see theoretical paper)
    tprice alpha() const
    {
       for(unsigned i=0; i<orderbook().A.size(); i++)
            if(orderbook().A[i].owner != fmyindex)
                return orderbook().A[i].price;
       return khundefprice;
    }

    /// returns current the-others-bid ¨(see theoretical paper)
    tprice beta() const
    {
       for(unsigned i=0; i<orderbook().B.size(); i++)
            if(orderbook().B[i].owner != fmyindex)
                return orderbook().B[i].price;
       return klundefprice;
    }
private:
    std::shared_ptr<const tmarketdata> fdata;
    unsigned fmyindex;
};

/// base class for strategies runable ED
class teventdrivenstrategy : public tstrategy
{
    friend class tmarket;
public:

    /// constructor
    /// \p interval - interval between calls to marketsim::teventdrivenstrategy::event
    /// (may be later changed by marketsim::teventdrivenstrategy::setinterval)
    /// \p warmingtime - the time befor marketsim::teventdrivenstrategy::event is called
    /// for the first time.
    teventdrivenstrategy(tabstime interval, tabstime warmingtime = 0) :
        tstrategy(warmingtime), finterval(interval) {}

    /// has to be implemented by any descendant.
    /// \p info is the current state of the market
    /// \p t is the absolute time
    /// \p resultoflast points to the result of processing of the last request
    /// (returned by the last run of this method) being null means that this is the first call of this method.
    virtual trequest event(const tmarketinfo& info, tabstime t, trequestresult* resultoflast) = 0;


    /// called by the simulator after the end of simulation
    virtual void sequel(const tmarketinfo&) {}
protected:
    /// accessor
    double interval() const { return finterval; }

    /// may be used to change the inter-event interval (note that the delay is measured from
    /// the end of execution of marketsim::teventdrivenstrategy::event)
    void setinterval(double ainterval)
    {
        finterval = ainterval;
    }

protected:
    void startlearning() { flastlearningstart = abstime(); }
    void stoplearning() { flastlearningend = abstime(); }

private:
//    tabstime step(tabstime t, bool firsttime);

    /// implemented for the strategy being runable RT
    virtual void trade(twallet) override
    {
        bool firsttime = true;
        while(!endoftrading())
        {
            trequestresult rr;
            tmarketinfo info = this->internalgetinfo<true>();
            tabstime t = abstime();
            rr = internalrequest<true>(event(info,t,firsttime ? 0 : &rr));
            firsttime = false;
            double dt = finterval;
            sleepuntil(t+dt);
        }
        tmarketinfo info = this->internalgetinfo<true>();
        sequel(info);
    }
    double finterval;

    tabstime flastlearningstart;
    tabstime flastlearningend;
};




/// class performing the simulation.
class tmarket : private chronos::Chronos
{
private:
    /// basic random generator
    std::uniform_real_distribution<double> funiform;

    /// used for randomized strategies as well as for small perturbation
    /// in marketsim::teventdrivenstrategy
    std::default_random_engine fengine;

    /// returns an uniform random variable
    double uniform()
    {
        return funiform(fengine);
    }

public:
    /// seeds a random generator. Called by marketsim::tmarket to distinguish individual runs
    void seed(unsigned i)
    {
        fengine.seed(i);
    }
private:

    friend class tstrategy;
    friend class tdsbase;

    /// converts a strategy \p id  to its index in marketsim::tmarketdata
    int findstrategy(const tstrategyid id)
    {
        assert(fmarketdata);
        for(unsigned i=0; i<fmarketdata->fstrategyinfos.size(); i++)
            if(id == fmarketdata->fstrategyinfos[i].strategyid())
                return i;
        throw marketsimerror("Internal error: cannot assign owner");
    }

    std::string findname(const tstrategyid id)
    {
        assert(fmarketdata);
        for(unsigned i=0; i<fmarketdata->fstrategyinfos.size(); i++)
            if(id == fmarketdata->fstrategyinfos[i].strategyid())
                return fmarketdata->fstrategyinfos[i].name();
        throw marketsimerror("Internal error: cannot assign name");
    }

    /// used by (friend class) marketsim::tstrategy
    tabstime getabstime()
    {
        return frunningwithchronos
                ? this->get_time() * fdef.ticktime()
                : fnonchronosstatreventtime + (::clock()-fclockstarteventtime) / CLOCKS_PER_SEC;
    }

    /// used by (friend class) marketsim::tstrategy
    tmarketinfo getinfo(tstrategyid id)
    {
        int owner = findstrategy(id);
        std::shared_ptr<tmarketdata> ssht = atomic_load(&fmarketsnapshot);
        assert(ssht);
        return tmarketinfo(ssht,owner);
    }

    /// schedules a request \p request by strategy \p strategyid to settle. The argument
    /// \p t is used only by ED version, telling the market the time.
    template <bool chronos>
    trequestresult settle(tstrategyid strategyid,
                                     const trequest& request,
                                     tabstime t = 0)
    {
        int owner = findstrategy(strategyid);
        if(islogging())
        {
            std::ostringstream s;
            s << "Called settle by " <<  findname(strategyid);

            std::ostringstream s2;
            s2 << "request: ";
               request.output(s2);
            possiblylog(floggingfilter.fsettle,0,s.str(),s2.str());
        }
        auto& ob = fmarketdata->forderbook;
        auto st = chronos ? getabstime() : t;
        try
        {
            trequestresult sr = ob.settle(
                           request,
                           fmarketdata->fstrategyinfos,
                           owner,
                           fmarketdata->ftimestamp++,
                           st);
            if(islogging())
            {
                std::ostringstream s;
                s << "Settle by " << findname(strategyid) << " finished";
                std::ostringstream s2;
                s2 << "q=" << sr.q;
                if(sr.errs.size())
                {
                    s2 << ", errs:";
                    for(unsigned i=0; i<sr.errs.size(); i++)
                        s2 << " '" << sr.errs[i].text << ",";
                }
                possiblylog(floggingfilter.fsettle,0,s.str(), s2.str());
            }
            auto profile = ob.obprofile();
            fmarketdata->fhistory.add(tsnapshot(ob.b(),ob.a(),st,sr.q,profile.B.volume(),profile.A.volume()));
            return sr;

        }
        catch(std::runtime_error& e)
        {
            possiblylog(true, 0,"settle throwed exception",e.what());
            throw marketsimerror(e.what());
        }

        catch(...)
        {
            possiblylog(true, 0,"settle throwed unknown exception");
            throw marketsimerror("unknown exception during settle");;
        }
    }

    /// copies the current state of the market to marketsim::tmarket::fmarketdata, which
    /// is a shared pointer; thus, once it is passed to some strategy and another shapshot
    /// is made meanwhile, the aparatus of shared pointers ensures proper memory management.
    void setsnapshot()
    {
        assert(fmarketdata);
        if(frunningwithchronos)
        {
            possiblylog(floggingfilter.fmarketdata,0,"New copy of tmarketdata...");
            atomic_store(&fmarketsnapshot, std::make_shared<tmarketdata>(*fmarketdata));
            possiblylog(floggingfilter.fmarketdata,0,"New copy of tmarketdata created");
        }
        else
            fmarketsnapshot = fmarketdata;
    }

    /// discendat of Chrnonos::Chronos::tick, called each time after all the Chrnonos::Workers
    /// are served. This method makes a new snapshot of the market \see marketsim::tmarket::setsnapshot
    virtual void tick() override
    {
        assert(fmarketdata);
        setsnapshot();
        fmarketdata->fextraduration.add(get_remaining_time().count());
        possiblylog(floggingfilter.ftick,0,"Tick called");
    }

    /// called when a strategy raises an exeption. In this case, this event is recorded to
    /// the corresponding strategy's info and the exception is muted.
    void reportexception(tstrategyid owner, const std::string& e)
    {
        assert(owner != nostrategy);
        try
        {
            auto s = findstrategy(owner);
            if(fmarketdata)
                fmarketdata->fstrategyinfos[s].reportexception(e);
        }
        catch(...)
        {

        }
    }
public:

    /// constructor
    /// \p maxtime - time of simulation in absolute time
    /// \p adef - paremeters of market
    tmarket(tabstime maxtime, tmarketdef adef) :
        chronos::Chronos(adef.chronosduration, maxtime / adef.ticktime()),
        fdef(adef), fmaxtime(maxtime),
        flog(0)
    {
    }

    /// destructor
    virtual ~tmarket() {}




    /// if called with \p log !=\c 0, executeion is logged into \p log, depending on
    /// \p filter.
    void setlogging(std::ostream& log, tloggingfilter filter)
    {
        flog = &log;
        floggingfilter = filter;
    }

    /// stops logging
    void resetlogging()
    {
        flog = 0;
    }

    /// changes the simulation definition

    /// \c true if logging is on
    bool islogging() const
    {
        return flog != 0;
    }

    /// accessor
    const tmarketdef& def() const
    {
        return fdef;
    }


    /// returns results of the last simulation
    std::shared_ptr<tmarketdata> results() const
    {
        return fmarketdata;
    }

    /// Run the simulation for stregeties in RT (\tparam chronos == \c true) or ED (\tparam chronos == \c false).
    /// The compening strategies are those associated with \p competitors,
    /// having \p endowments. The parameter \p garbage is needed for technical reasons,
    /// namely to store strategies, which failed to release control (\c chronos==true only).
    /// \return vector of booleans, corresponding to individual strategies, with \c true
    /// meaning that the corresponding strategy failed to release control.
    template <bool chronos=true, typename D=tnodemandsupply, bool allowlearning=false>
    std::vector<bool> run(std::vector<competitorbase<chronos>*> competitors,
             std::vector<twallet> endowments,
             std::vector<tstrategy*>& garbage)
    {
        assert(endowments.size()==competitors.size());
        if(islogging() && fdef.directlogging)
            *flog << flogheader << std::endl;
        frunningwithchronos = chronos;
        possiblylog(true,0,"Starting simulation");
        if(fdef.directlogging)
            possiblylog(true,0,"Direct logging","Only entries by fmarket are logged due to thread safety.");
        std::vector<tstrategy*> strategies;
        tdsbase* ds=0;
        std::vector<std::string> names;
        try
        {
            for(unsigned i=0; i<competitors.size(); i++)
            {
                strategies.push_back(competitors[i]->create());
                names.push_back(competitors[i]->name());
            }
            ds = new D;
        }
        catch (...)
        {
            if(ds)
                delete ds;
            for(unsigned i=0; i<strategies.size(); i++)
                delete strategies[i];
            throw;
        }
        ds->fmarket = this;
        fmarketdata.reset(new tmarketdata(endowments,strategies,ds,names));
        setsnapshot();
        chronos::workers_list wl;
        unsigned i=0;
        for(; i<strategies.size(); i++)
        {
            wl.push_back(strategies[i]);
            strategies[i]->fendowment = endowments[i];
            strategies[i]->fmarket = this;
            if(!(competitors[i]->startswithoutwarmup()))
                strategies[i]->fwarmingtime += fdef.warmuptime;
        }
        bool waserror = true;
        std::string errtxt;
        try
        {
            if constexpr(chronos)
            {
                chronos::Chronos::run(wl);
            }
            else
            {
                auto n = strategies.size();
                std::vector<bool> firsttime(n,true);
                std::vector<trequestresult> results(n);

                tabstime T = get_max_time() * def().ticktime();
                std::vector<tabstime> ts;
                for(unsigned i=0; i<n; i++)
                    ts.push_back(strategies[i]->fwarmingtime);
                std::vector<tabstime> rts(n,std::numeric_limits<tabstime>::max());
                std::vector<trequest> rs(n);
                tabstime dst = fdef.demandupdateperiod;
                for(;;)
                {
                    unsigned first;
                    bool isevent;
                    tabstime t = std::numeric_limits<tabstime>::max();
                    for(unsigned i=0; i<n; i++)
                    {
                        if(ts[i] <= t)
                        {
                            t = ts[i];
                            first = i;
                            isevent = true;
                        }
                        if(rts[i] <= t)
                        {
                            t = rts[i];
                            first = i;
                            isevent = false;
                        }
                    }
                    bool dsevent;
                    if(dst < t)
                        dsevent = true;
                    else
                        dsevent = false;


                    if(t >= T)
                        break;
//std::cout << "t=" << t << ", dst=" << dst << std::endl;
                    if(dsevent)
                    {
                        auto ds = fmarketdata->fds->delta(dst, *fmarketdata.get() );
                        distributeds(ds,strategies,dst);
                        dst += fdef.demandupdateperiod;
                    }
                    else
                    {
                        teventdrivenstrategy* str = (static_cast<teventdrivenstrategy*>(strategies[first]));

                        if(isevent)
                        {
                           auto info = str->internalgetinfo<false>();
                           fclockstarteventtime = clock();
                           fnonchronosstatreventtime = t;

                           rs[first] = str->event(info,t,firsttime[first] ? 0 : &(results[first]));

//                           double endt = ;
                           double dt = (::clock()-fclockstarteventtime) / CLOCKS_PER_SEC;

                           if constexpr(allowlearning)
                           {
                               if(str->flastlearningstart >= t && str->flastlearningend > str->flastlearningstart)
                               {
                                   assert(str->flastlearningend <= t+dt);
                                   dt -= str->flastlearningend - str->flastlearningstart;
                                   assert(dt >= 0);
                               }
                           }

                           fmarketdata->fstrategyinfos[first].addcomptime(dt);
                           ts[first] = t + std::max(dt,str->finterval) + def().ticktime()
                                                 + str->uniform() * def().epsilon;
                           rts[first] = t + dt;
    // std::cout << " calling event of strategy " << first << std::fixed << " at " << t  << "s took " << dt << "s" << std::endl;
                           if(islogging())
                           {
                               std::ostringstream s;
                               s << "duration " << dt;
                               possiblylog(floggingfilter.frequest,str->fid,"event (non-chronos)",s.str());
                           }

                           firsttime[first] = false;
                        }
                        else
                        {
                            if(islogging())
                            {
                                possiblylog(floggingfilter.frequest,str->fid,"non-chronos request","");
                            }

                           results[first]=str->internalrequest<false>(rs[first],t);
                           rts[first] = std::numeric_limits<tabstime>::max();
                           setsnapshot();
                        }
                    } // dsevent
                }
                for(unsigned i=0; i<n; i++)
                {
                    teventdrivenstrategy* str
                            = (static_cast<teventdrivenstrategy*>(strategies[i]));
                    auto info = str->internalgetinfo<false>();
                    str->sequel(info);
                }
            }
            waserror = false;
        }
        catch (chronos::error_already_finished)
        {
            errtxt = "run throwed internal error: uncaught error_already_finished";
        }
        catch (marketsim::error& e)
        {
            errtxt = std::string("run throwed marketsime error: ") + e.what();
        }
        catch (chronos::error& e)
        {
            errtxt = std::string("run throwed chronos error: ") + e.what();
        }
        catch (...)
        {
            errtxt = "run throwed unknown error.";
        }

        if(islogging() && fdef.loggingfilter.fprotocol)
        {
            fmarketdata->protocol(*flog,fmaxtime,fdef.numsnapshotsinprotocol);
        }

        if(islogging() && !fdef.directlogging)
        {
            *flog << flogheader << std::endl;
            for(unsigned i=0; i<strategies.size(); i++)
                *flog << fmarketdata->fstrategyinfos[i].fsublog.str();
            *flog << fmarketdata->fmarketsublog.str();
        }

        bool finished;
        unsigned tickstowait = def().timeowaitafterend / def().ticktime();
        for(unsigned i=0; i<tickstowait; i++)
        {
            finished = true;
            for(unsigned j=0; j<strategies.size(); j++)
            {
                if(strategies[j]->still_running())
                {
                     finished = false;
                     break;
                }
            }
            if(finished)
                break;
            std::this_thread::sleep_for(def().chronosduration);
        }
        std::vector<bool> ret(strategies.size());

        for(unsigned i=0; i<strategies.size(); i++)
        {
            if(strategies[i]->still_running())
            {
                fmarketdata->fstrategyinfos[i].reportoverrun();
                garbage.push_back(strategies[i]);
                ret[i] = true;
            }
            else
            {
                delete strategies[i];
                ret[i] = false;
            }
        }
        if(waserror)
            throw runerror(errtxt);
        return ret;
    }


private:

    /// header for log
    static constexpr const char* flogheader = "what;strategyindex;strategyname;abstime;explanation;chronotime;computertime;"
                                              "timestamp;b(snap);a(snap);money;stocks;"
                                              ";A;;B;";


    /// state variable: market definition
    tmarketdef fdef;

    /// time the simulation takse
    tabstime fmaxtime;

    /// state variable: pointer to log, if any
    std::ostream* flog;
    /// state variable
    tloggingfilter floggingfilter;
    /// state variables
    bool frunningwithchronos;
    double fclockstarteventtime;
    tabstime fnonchronosstatreventtime;

    /// method routinely called on potential logging
    void possiblylog(bool doit, tstrategyid owner, const std::string&
                 shortmsg, const std::string& longmsg = "")
    {
        if(doit && islogging() && fmarketdata)
        {
            if(fdef.directlogging && frunningwithchronos && owner != nostrategy)
                return;
            int sn = owner != nostrategy ? findstrategy(owner) : 0;
            assert(sn < fmarketdata->fstrategyinfos.size());
            std::ostream& o = fdef.directlogging ? *flog ://std::cout;
                                       (owner!=nostrategy
                                         ? fmarketdata->fstrategyinfos[sn].fsublog
                                         : fmarketdata->fmarketsublog);
            o << shortmsg << ";";
            if(owner)
                o << sn << ";" << fmarketdata->fstrategyinfos[sn].name() << ";";
            else
                o << ";marketsim;";
            o << getabstime() << ";"
              << longmsg << ";";

            if(frunningwithchronos)
                o << get_time();
            o << ";" << (::clock() - fmarketdata->fstartclocktime) /CLOCKS_PER_SEC << ";"
              << fmarketdata->ftimestamp << ";";

            std::shared_ptr<tmarketdata> ssht = atomic_load(&fmarketsnapshot);
            if(ssht)
               o << p2str(ssht->forderbook.b()) << ";"
                 << p2str(ssht->forderbook.a()) << ";";
             else
               o << ";;";

            if(owner!=nostrategy)
            {
                twallet w = fmarketdata->fstrategyinfos[sn].wallet();
                o << w.money() << ";" << w.stocks() << ";";
            }
            else
                o << ";;";
            if(owner==nostrategy)
            {
                const auto& p = fmarketdata->forderbook.obprofile();
                o << "A;";
                p.A.output(o);
                o << ";"
                  << "B;";
                p.B.output(o);
                o << ";";
            }
            o << std::endl;
        }
    }
    void distributeds(const tdsrecord& ds, const std::vector<tstrategy*> strategies, tabstime t)
    {
        if(ds.d > 0)
        {
            unsigned numd = 0;
            for(unsigned i=0; i< strategies.size(); i++)
                if(strategies[i]->acceptsdemand())
                    numd++;
            if(numd > 0)
            {
                unsigned theone = numd*uniform();
                for(unsigned i=0; i< strategies.size(); i++)
                {
                    if(strategies[i]->acceptsdemand())
                    {
                        if(theone==0)
                        {
                            int owner = findstrategy(strategies[i]->fid);
                            std::ostringstream s1;
                            s1 << "Demand distributed to " << findname(strategies[i]->fid);
                            std::ostringstream s2;
                            s2 << "Demand: " << ds.d;

                            possiblylog(floggingfilter.fds,0,s1.str(),s2.str());
                            fmarketdata->fstrategyinfos[owner].addds(ds.d,0,t);
                            break;
                        }
                        else
                            theone--;
                    }
                }
            }
            else
                throw marketsimerror("No strategy to pass demand to");
        }
        if(ds.s>0)
        {
            unsigned nums = 0;
            for(unsigned i=0; i< strategies.size(); i++)
                if(strategies[i]->acceptssupply())
                    nums++;
            if(nums > 0)
            {
                unsigned theone = nums*uniform();
                for(unsigned i=0; i< strategies.size(); i++)
                {
                    if(strategies[i]->acceptssupply())
                    {
                        if(theone==0)
                        {
                            int owner = findstrategy(strategies[i]->fid);
                            std::ostringstream s1;
                            s1 << "Supply distributed to " << findname(strategies[i]->fid);
                            std::ostringstream s2;
                            s2 << "Supply: " << ds.s;

                            possiblylog(floggingfilter.fds,0,s1.str(),s2.str());
                            fmarketdata->fstrategyinfos[owner].addds(0,ds.s,t);
                            break;
                        }
                        else
                            theone--;
                    }
                }
            }
            else
                throw marketsimerror("No strategy to pass demand to");
        }
    }


    /// all the data regarding current icmulation
    std::shared_ptr<tmarketdata> fmarketdata;
    /// current market snapshot
    std::shared_ptr<tmarketdata> fmarketsnapshot;
};


template<bool demand, bool supply>
class tdsprocessingstrategy : public teventdrivenstrategy
{
public:
    tdsprocessingstrategy()
        : teventdrivenstrategy(0), flastdssize(0)
        { static_assert(demand || supply); }

    virtual trequest dsevent(const tdsevent&, const tmarketinfo& info, tabstime t) = 0;

    virtual trequest event(const tmarketinfo& info, tabstime t, trequestresult* lrr)
    {
        trequest ret;
        const auto& h = info.myinfo().dshistory().x();
        int n = h.size();
        if(n > flastdssize)
        {
            auto ds = h[flastdssize++];
            ret = dsevent(ds,info,t);
        }
        setinterval(flastdssize == n ? this->market()->def().demandupdateperiod : 0);
        return ret;
    }

private:
    int flastdssize;

    virtual bool acceptsdemand() const  { return demand; }
    virtual bool acceptssupply() const  { return supply; }

};

inline void tstrategy::main()
{
    try {
        sleepfor(fwarmingtime);
        trade(fendowment);
    }
    catch (chronos::error_already_finished)
    {
        fmarket->possiblylog(true,fid, "stopped: already finished");
        if(fmarket->get_time() < fmarket->get_max_time()-1)
            throw;
        else
            return;
    }
    catch (marketsim::error)
    {
        fmarket->possiblylog(true,fid, "main throwed marketsim error");
        throw;
    }
    catch (chronos::error)
    {
        fmarket->possiblylog(true, fid, "main throwed chronos error");
        throw;
    }
    catch (std::runtime_error& e)
    {
        fmarket->possiblylog(true, fid, "main throwed error", e.what());
        fmarket->reportexception(fid,e.what());
        return;
    }
    catch (...)
    {
        fmarket->possiblylog(true, fid, "main throwed unknown error");
        fmarket->reportexception(fid,"unknown error");
        return;
    }
}


template <bool chronos>
inline trequestresult tstrategy::internalrequest(const trequest& request, tabstime t)
{
    assert(fmarket);
    trequestresult ret;
    try
    {
        trequestresult ret;
        if(!request.empty())
        {
            if(fmarket->islogging())
            {
                std::ostringstream s;
                request.output(s);
                fmarket->possiblylog(fmarket->floggingfilter.frequest,fid,"request called",s.str());
            }

            if constexpr(chronos)
            {
                ret = fmarket->async([this,&request]()
                {
                    return this->fmarket->settle<true>(this->fid,request);
                });
            }
            else
                ret = this->fmarket->settle<false>(fid,request,t);

            if(fmarket->islogging())
            {
                std::ostringstream s;
                if(ret.errs.size()==0)
                    s << "OK";
                else
                    for(unsigned i=0; i<ret.errs.size(); i++)
                        s << ret.errs[i].text << ",";
                // tbd return wallet...
                fmarket->possiblylog(fmarket->floggingfilter.frequest,fid, "settle returned", s.str());
            }
        }
        return ret;
    }
    catch (chronos::error_already_finished)
    {
        fmarket->possiblylog(true,fid, "settle throwed already finished");
        throw;
    }
    catch (chronos::error& e)
    {
        fmarket->possiblylog(true,fid, "settle throwed chronos error", e.what());
        throw;
    }
    catch (std::runtime_error& e)
    {
        fmarket->possiblylog(true,fid, "settle throwed error", e.what());
        throw marketsimerror(e.what());
    }

    catch (...)
    {
        fmarket->possiblylog(true,fid, "settle throwed unknown error");
        throw marketsimerror( "settle throwed unknown error");
    }
}



template <bool chronos>
tmarketinfo tstrategy::internalgetinfo()
{
    assert(fmarket);
    try
    {
        fmarket->possiblylog(fmarket->floggingfilter.fgetinfo, fid,"Calling getinfo");
        auto ret =  this->fmarket->getinfo(fid);
        fmarket->possiblylog(fmarket->floggingfilter.fgetinfo, fid,"getinfo called");
        return ret;
    }
    catch (chronos::error& e)
    {
        fmarket->possiblylog(true,fid,"getinfo throwed chronos error",e.what());
        throw;
    }
    catch (std::runtime_error& e)
    {
        fmarket->possiblylog(true, fid,"getinfo throwed error",e.what());
        throw marketsimerror(e.what());
    }
    catch(...)
    {
        fmarket->possiblylog(true, fid,"getinfo throwed unknown error");
        throw marketsimerror("getinfo throwed unknown error");
    }

}


inline tmarketinfo tstrategy::getinfo()
{
    return internalgetinfo<true>();
}


inline void tstrategy::sleepfor(tabstime t)
{
    try
    {
        if(fmarket->islogging())
        {
            std::ostringstream s;
            s << "sleepfor(" << t << ") called";
            fmarket->possiblylog(fmarket->floggingfilter.fsleep, fid, s.str());
        }
        sleep_until(fmarket->get_time() + t/fmarket->def().ticktime());
        fmarket->possiblylog(fmarket->floggingfilter.fsleep,fid, "sleepfor finished");
    }
    catch (chronos::error& e)
    {
        fmarket->possiblylog(true,fid,"sleepfor throwed chronos error",e.what());
        throw;
    }
    catch (std::runtime_error& e)
    {
        fmarket->possiblylog(true, fid,"sleepfor throwed error",e.what());
        throw marketsimerror(e.what());
    }
    catch(...)
    {
        fmarket->possiblylog(true, fid,"sleepfor throwed unknown error");
        throw marketsimerror("getinfo throwed unknown error");
    }

}

inline void tstrategy::sleepuntil(tabstime t)
{
    try
    {
        if(fmarket->islogging())
        {
            std::ostringstream s;
            s << "sleepuntil(" << t << ") called";
            fmarket->possiblylog(fmarket->floggingfilter.fsleep,fid, s.str());
        }
        sleep_until(t/fmarket->def().ticktime());
        fmarket->possiblylog(fmarket->floggingfilter.fsleep,fid, "sleepuntil finished");
    }
    catch (chronos::error& e)
    {
        fmarket->possiblylog(true,fid,"sleepuntil throwed chronos error",e.what());
        throw;
    }
    catch (std::runtime_error& e)
    {
        fmarket->possiblylog(true,fid,"sleepuntil throwed error",e.what());
        throw marketsimerror(e.what());
    }
    catch(...)
    {
        fmarket->possiblylog(true,fid,"sleepuntil throwed unknown error");
        throw marketsimerror("getinfo throwed unknown error");
    }

}

inline tabstime tstrategy::abstime()
{
    try
    {
        tabstime at = fmarket->getabstime();

        if(fmarket->islogging())
        {
            std::ostringstream s;
            s << "abstime called returning " << at;
            fmarket->possiblylog(fmarket->floggingfilter.fabstime,fid, s.str());
        }
        return at;
    }
    catch (chronos::error& e)
    {
        fmarket->possiblylog(true,fid,"abstime throwed chronos error",e.what());
        throw;
    }
    catch (std::runtime_error& e)
    {
        fmarket->possiblylog(true,fid,"abstime throwed error",e.what());
        throw marketsimerror(e.what());
    }
    catch(...)
    {
        fmarket->possiblylog(true,fid,"abstime throwed unknown error");
        throw marketsimerror("getinfo throwed unknown error");
    }

}

inline std::default_random_engine& tstrategy::engine()
{
    assert(fmarket);
    return fmarket->fengine;
}

inline double tstrategy::uniform()
{
    assert(fmarket);
    return fmarket->funiform(fmarket->fengine);
}


inline bool tstrategy::endoftrading()
{
    return !ready();
}

inline void tstrategy::possiblylog(const std::string&
             shortmsg, const std::string& longmsg)
{
    auto f = fmarket->def().loggedstrategies;
    auto ind = fmarket->findstrategy(fid);
    for(unsigned i=0; i<f.size(); i++)
        if(f[i]==ind)
             fmarket->possiblylog(true, fid, shortmsg, longmsg);
}


template <bool logging>
inline int tmarketdef::findduration(unsigned nstrategies, const tmarketdef& def, std::ostream& log)
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
            m.setlogging(o,def.loggingfilter);
        }

        // tbd create infinitywallet
        std::vector<twallet> e(nstrategies,twallet::infinitewallet());
        competitor<calibratingstrategy> c;
        std::vector<competitorbase<true>*> s;
        for(unsigned j=0; j<nstrategies; j++)
            s.push_back(&c);
        try {
            log << "d = " << d << std::endl;
            m.run(s,e,garbage);

            if(garbage.size()) /// tbd speciální výjimka
                throw marketsimerror("Internal error: calibrating strategy unterminated.");
            double rem = m.results()->fextraduration.average();
            log << "remaining = " << rem << std::endl;
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
    log << d << " found suitable " << std::endl;
    return d;
}

inline std::default_random_engine& tdsbase::engine()
{
    assert(fmarket);
    return fmarket->fengine;
}

inline double tdsbase::uniform()
{
    assert(fmarket);
    return fmarket->funiform(fmarket->fengine);
}


} // namespace

#endif // MARKETSIM_HPP
