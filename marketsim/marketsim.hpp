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
static constexpr tprice kminprice = 0;
static constexpr tprice kinfprice = std::numeric_limits<tprice>::max();
static constexpr tprice kminfprice = -1;

using tvolume = int;
using ttime = double;
static constexpr ttime kmaxtime = std::numeric_limits<ttime>::max();

inline std::string p2str(tprice p)
{
    if(p==kminfprice)
        return "lundef";
    else if(p==kinfprice)
        return "hundef";
    else
    {
        std::ostringstream os;
        os << p;
        return os.str();
    }
}


struct tmarketdef
{
    const double flattency = 0.01;
    const double minupdateinterval = 0.001;
};

class twallet
{
public:
    twallet() : fmoney(0), fstocks(0) {}
    twallet(tprice amoney, tvolume astocks) : fmoney(amoney), fstocks(astocks) {}
    tprice& money() { return fmoney; }
    tvolume& stocks() { return fstocks; }
    const tprice& money() const  { return fmoney; }
    const tvolume& stocks() const { return fstocks; }
private:
    tprice fmoney;
    tvolume fstocks;
};

/*

template <typename T>
class tdynamicarray
{
   const T& operator [] (unsigned i) const
       { return i < fx.size() ? fx[i] : 0; }
   T& operator [] (unsigned i)
   {
       if(i >= fx.size())
           fx.resize(i+1,0);
       return fx[i];
   }
private:
   std::vector<T> fx;
};

*/

struct tpreorder
{
    tprice price;
    tvolume volume;
    tpreorder(tprice aprice, tvolume avolume) : price(aprice), volume(avolume)
    {
    }
    template <bool ask>
    static tpreorder marketorder(tvolume avolume)
    {
        if constexpr(ask)
            return tpreorder(kminfprice,avolume);
        else
            return tpreorder(kinfprice,avolume);
    }

    template <bool ask>
    bool ismarket() const
    {
        if constexpr(ask)
        {
            assert(price < kinfprice);
            return price == kminfprice;
        }
        else
        {
            assert(price > kminfprice);
            return price == kinfprice;
        }
    }
};

inline std::ostream& operator << (std::ostream& o, const tpreorder& r)
{
    o << "(";
    switch(r.price)
    {
        case kminfprice:
            o << "sell market";
            break;
        case kinfprice:
            o << "buy market";
            break;
        default:
            o << r.price;
    }

    o << "," << r.volume << ")";
    return o;
}

struct torder : public tpreorder
{
    ttime time;
    unsigned owner;
    torder(tprice aprice, tvolume avolume, ttime atime, unsigned aowner)
        : tpreorder(aprice, avolume), time(atime), owner(aowner) {}
};


inline std::ostream& operator << (std::ostream& o, const torder& r)
{
    o << "(";
    switch(r.price)
    {
        case kminfprice:
            o << "sell market";
            break;
        case kinfprice:
            o << "buy market";
            break;
        default:
            o << r.price;
    }

    o << "," << r.volume << "," << r.time << "," << r.owner << ")";
    return o;
}

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

template <typename O, bool ask>
class tsortedordercontainer
{
public:
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

    O& operator [](unsigned i)
    {
        assert(i < fx.size());
        return fx[i];
    }

    const O& operator [](unsigned i) const
    {
        assert(i < fx.size());
        return fx[i];
    }

    unsigned size() const
    {
        return fx.size();
    }

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

template <typename C, bool ask>
class tlistbase : public C
{
public:

    tlistbase(const C& ac) : C(ac) {}
    tlistbase()  {}


    tprice maxprice() const
    {
        if constexpr(ask)
        {
            if(this->size()==0)
                return kminfprice;
            else
                return (*this)[this->size()-1].price;
        }
        else
        {
            if(this->size()==0)
                return kminfprice;
            else
                return (*this)[0].price;
        }
    }

    tprice minprice() const
    {
        if constexpr(ask)
        {
            if(this->size()==0)
                return kinfprice;
            else
                return (*this)[0].price;
        }
        else
        {
            if(this->size()==0)
                return kinfprice;
            else
                return (*this)[this->size()-1].price;
        }
    }
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

template <typename C, bool ask>
inline std::ostream& operator << (std::ostream& o, const tlistbase<C,ask>& v)
{
    for(unsigned i=0; i<v.size(); i++)
        o << v[i] << ",";
    return o;
}

template <bool ask>
using tpreorderlist=tlistbase<tsortedordercontainer<tpreorder,ask>,ask>;

template <bool ask>
using torderlist=tlistbase<tsortedordercontainer<torder,ask>,ask>;

template <bool ask>
using torderptrlist=tlistbase<torderptrcontainer<torder>,ask>;


template <typename LB, typename LA>
class tprofilebase
{
public:
    LB B;
    LA A;

    void setlists(const LB& ab, const LA& aa)
    {
        B = ab;
        A = aa;
    }
    tprice a() const
    {
        return A.minprice();
    }
    tprice b() const
    {
        return B.maxprice();
    }
    void clear()
    {
        B.clear();
        A.clear();
    }
    bool check() const
    {
        tprice ap = a();
        tprice bp = b();
        return  a() > b() || (bp == kinfprice && ap==kinfprice) ||
                (bp == kminfprice && ap==kminfprice);
    }
};

using torderprofile = tprofilebase<torderlist<false>,torderlist<true>>;
using tpreorderprofile = tprofilebase<tpreorderlist<false>,tpreorderlist<true>>;
using torderptrprofile = tprofilebase<torderptrlist<false>,torderptrlist<true>>;

/*class torderptrprofile: public tprofilebase<torderptrlist<false>,torderptrlist<true>>
{
public:
    torderptrprofile(const std::vector<torder*>& ab,const std::vector<torder*>& aa) :
        tprofilebase<torderptrlist<false>,torderptrlist<true>>
           (torderptrlist<false>(ab),torderptrlist<true>(aa))
    {

    }
};*/


class trequest
{
public:
    struct teraserequest
    {
        teraserequest(bool aall) : all(aall) {}
        bool all;
        std::vector<bool> a;
        std::vector<bool> b;
        bool possiblyerase() const
        {
            return all || b.size() || a.size();
        }
    };

    enum ewarning { OK, ecrossedorders, enotenoughmoneytobuy, enotenoughstockstosell,
                    enotenoughmoneytoput, enotenoughstockstoput,
                    enumwargnings};

    static std::string warninglabel(ewarning w)
    {
        static std::vector<std::string> lbls =
        { "OK" , "crossed orders", "not enough moneyto buy",
          "not enough stocks to sell", "not enough money to put",
          "not enough stocks to put" };
        return lbls[w];
    }

    trequest(const tpreorderprofile& orderrequest,
             const teraserequest& eraserequest = teraserequest(true),
             ttime nextupdate = 0,
             tprice consumption = 0) :
        fconsumption(consumption),
        fnextupdate(nextupdate),
        forderrequest(orderrequest),
        feraserequest(eraserequest)
    {}
    trequest() : fconsumption(0), fnextupdate(0),
        forderrequest(), feraserequest(false) {}
    tprice consumption() const { return fconsumption; }
    const tpreorderprofile& requestprofile() const { return forderrequest; }
    const teraserequest& eraserequest() const { return feraserequest; }
    ttime nextupdate() const { return fnextupdate; }
public:
    tprice fconsumption;
    ttime fnextupdate;
    tpreorderprofile forderrequest;
    teraserequest feraserequest;
};


class tmarkethistory
{
    friend class tmarketsimulation;
public:
    struct tsnapshot
    {
        tprice b;
        tprice a;
        ttime t;
        double p() const
        {
            if(b==kminfprice || a==kminfprice)
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
    tmarkethistory() : fx({{kminfprice,kinfprice,0}}) {}

    tsnapshot operator () (ttime at) const
    {
        tsnapshot val={0,0,at};
        auto it = std::lower_bound(fx.begin(),fx.end(),val,cmp);
        if(it==fx.end())
             it--;
        return *it;
    }

    tprice b(ttime at) const
    {
        return (*this)(at).b;
    }

    tprice a(ttime at) const
    {
        return (*this)(at).a;
    }

    double p(ttime at) const
    {
        return (*this)(at).p();
    }

    void add(const tsnapshot& s)
    {
        fx.push_back(s);
    }

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

class tmarketinfo
{

public:
    const tmarkethistory& history;
    const torderptrprofile& orderbook;
    const torderprofile& myprofile;
    unsigned myid;
    ttime t;
};

class ttradinghistory
{
public:
    struct tconsumptionevent
    {
        ttime ftime;
        tprice famount;
    };

    struct tradingevent
    {
        ttime ftime;
        tprice fmoneydelta;
        tvolume fstockdelta;
        unsigned partner;
    };

    ttradinghistory(const std::string& name, const twallet& wallet) :
        fname(name), fwallet(wallet), fblockedmoney(0), fblockedstocks(0) {}
    void addconsumption(tprice c, ttime t)
    {
        assert(c > fwallet.money());
        fconsumption.push_back(tconsumptionevent({t,c}));
        fwallet.money() -= c;
    }
    void addtrade(tprice moneydelta, tvolume stockdelta, ttime t, unsigned partner)
    {
        assert(fwallet.money() + moneydelta >= 0);
        assert(fwallet.stocks() + stockdelta >= 0);
        ftrading.push_back({t, moneydelta, stockdelta, partner});
        fwallet.money() += moneydelta;
        fwallet.stocks() += stockdelta;
    }
    const twallet& wallet() const { return fwallet; }
    twallet& wallet() { return fwallet; }
    const tprice& blockedmoney() const { return fblockedmoney; }
    tprice& blockedmoney() { return fblockedmoney; }
    tprice availablemoney() { return fwallet.money() - fblockedmoney;}
    const tvolume& blockedstocks() const { return fblockedstocks; }
    tvolume& blockedstocks() { return fblockedstocks; }
    tvolume availablevolume() { return fwallet.stocks() - fblockedstocks;}
    const std::vector<tradingevent>& trading() const { return ftrading; }
    const std::string& name() const { return fname; }

    enum eoutputlevel { enone, ewallet, etrades, enumoutputlevels};
    void output( std::ostream& o, eoutputlevel level) const
    {
        if(level > enone)
            o << "Results of strategy " << name() << std::endl;

        if(level >= ewallet)
        {
            o << "Money: " << wallet().money()
                             << ", stocks: " << wallet().stocks() << std::endl;
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

/// Base class for strategies
class tstrategy
{
    friend class tsimulation;
protected:
    tstrategy(const std::string& name) : fname(name) {}
public:
    const std::string& name() const { return fname;}
private:
    virtual trequest event(const tmarketinfo& ai,
                           const ttradinghistory& aresult)  = 0;
    virtual void atend(const ttradinghistory&) {}
    virtual void warning(trequest::ewarning code, const std::string& txt)
    {
        std::ostringstream str;
        str << "Warning: " << trequest::warninglabel(code) << ": " << txt << std::endl;
//        throw str.str();
        orpp::sys::log();
    }
private:
    std::string fname;
};

class tsimulationresult
{
public:
    const tmarkethistory& history;
    const std::vector<ttradinghistory>& tradinghistory;
};

/// Class providing the simulation
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
                    profiles[owner].blockedmoney() -= fbook[owner].A.value(afilter);
                    fbook[owner].A.clear(afilter);
                }
                if(eraserequest.all || bfilter.size())
                {
                    profiles[owner].blockedstocks() -= fbook[owner].B.volume(bfilter);
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
                        profiles[fb[j]->owner].blockedmoney() -= price * toexec;
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
                return kinfprice;
        }
        tprice b() const
        {
            if(fb.size())
                return fb[0]->price;
            else
                return kminfprice;
        }


        torderptrprofile obprofile()
        {
            torderptrprofile ret;
            torderptrlist<false> bl(&fb);
            torderptrlist<true> al(&fa);
            ret.setlists(bl,al);
            return ret;
        }

    private:
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

public:


    static tsimulation& self()
    {
        static tsimulation lself;
        return lself;
    }

    static void setlogging(bool alogging)
    {
        self().flogging = alogging;
    }

    static void addstrategy(tstrategy& s, twallet e)
    {
        self().fstrategies.push_back(&s);
        self().fendowments.push_back(e);
    }

    static unsigned numstrategies()
    {
        return self().fstrategies.size();
    }

    static tsimulationresult results()
    {
        tsimulationresult ret ={ self().fhistory, self().fresults };
        return ret;
    }

    /// Rund the simulation. The simulation takes \p timeofrun seconds.
    static void run(double timeofrun)
    {
        self().dorun(timeofrun);
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

//             ttime newt = t + tickdistribution(orpp::sys::engine());
             for(unsigned i=0; i<n; i++)
             {
                 std::vector<unsigned> requesting;
                 if(alarmclock[i] < t+std::numeric_limits<double>::epsilon())
                 {
                     auto& r = fresults[i];
                     if(flogging)
                     {
                        orpp::sys::log() << "Calling stategy " << i << " at " << t << std::endl;
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
                     }
                     requesting.push_back(i);
                 }
                 std::shuffle(requesting.begin(), requesting.end(), orpp::sys::engine());
                 for(unsigned i=0; i<requesting.size(); i++)
                 {
                      std::vector<settlewarning> sr = ob.settle(orderrequests[requesting[i]],
                              fresults,
                              requesting[i],
                              t
                              );
                      for(auto r: sr)
                          fstrategies[requesting[i]]->warning(r.w, r.text);
                      t+=tickdistribution(orpp::sys::engine());
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
                        orpp::sys::log() << "Strategy " << i << std::endl;
                        orpp::sys::log() << '\t' << "A: " << ob.profile(i).A << std::endl;
                        orpp::sys::log() << '\t' << "B: " << ob.profile(i).B << std::endl;
                    }
                    orpp::sys::log() <<  std::endl;
                 }
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

class mmstrategy: public tstrategy
{
public:
    mmstrategy(const std::string& name)
     : tstrategy(name)
    {}


private:
    virtual trequest event(const tmarketinfo& mi,
                           const ttradinghistory& )
    {
        std::vector<bool> ae;
        std::vector<bool> be;

        trequest::teraserequest er(false);
        tpreorderprofile rp;

        assert(mi.myprofile.A.size()<=1);
        assert(mi.myprofile.B.size()<=1);

        tprice othersa = kinfprice;
        for(unsigned i=0; i<mi.orderbook.A.size(); i++)
        {
            auto r = mi.orderbook.A[i];
            if(r.owner != mi.myid)
            {
                othersa = r.price;
                break;
            }
        }

        tprice othersb = kminfprice;

        for(unsigned i=0; i<mi.orderbook.B.size(); i++)
        {
            auto r = mi.orderbook.B[i];
            if(r.owner != mi.myid)
            {
                othersb = r.price;
                break;
            }
        }

        tprice myolda = mi.myprofile.a();
        tprice myoldb = mi.myprofile.b();

        if(othersa != kinfprice && othersb != kminfprice)
        {
            tprice mynewa = othersa-1 > othersb ? othersa - 1 : othersa;
            tprice mynewb = othersb+1 < othersa ? othersb + 1 : othersb;
            if(mynewa == mynewb)
            {
                if(orpp::sys::uniform() > 0.5)
                    mynewa++;
                else
                    mynewb--;
            }
            if(mynewa != myolda)
            {
                if(myolda != kinfprice)
                    er.a.push_back(true);
                rp.A.add({mynewa,1});
            }
            if(mynewb != myoldb)
            {
                if(myoldb != kminfprice)
                    er.b.push_back(true);
                rp.B.add({mynewb,1});
            }

        }

        return trequest(rp,er,0);
    }


    virtual void atend(const ttradinghistory&)
    {

    }

    virtual void warning(trequest::ewarning, const std::string&)
    {
    }

};

/// Simplified version of \ref tstrategy
class tsimplestrategy : public tstrategy
{
public:
    struct tsimpleorderprofile
    {
        tpreorder b = tpreorder(kminfprice,0);
        tpreorder a = tpreorder(kinfprice,0);
        tvolume mb = 0;
        tvolume ma = 0;
    };
    static tsimpleorderprofile buymarket(tvolume volume)
    {
        tsimpleorderprofile s;
        s.mb = volume;
        return s;
    }
    static tsimpleorderprofile sellmarket(tvolume volume)
    {
        tsimpleorderprofile s;
        s.ma = volume;
        return s;
    }
    static tsimpleorderprofile noorder()
    {
        tsimpleorderprofile s;
        return s;
    }
    ttime timeinterval() const { return finterval; }
protected:
    tsimplestrategy(const std::string& name, ttime timeinterval)
        : tstrategy(name), finterval(timeinterval) {}
private:
    virtual tsimpleorderprofile simpleevent(const tmarketinfo& mi,
                                     const ttradinghistory& aresult ) = 0;

    virtual trequest event(const tmarketinfo& mi,
                                  const ttradinghistory& aresult )
    {
        tsimpleorderprofile r = simpleevent(mi, aresult) ;
        trequest::teraserequest er(false);
        tpreorderprofile pp;

        if(r.mb > 0)
            pp.B.add(torder::marketorder<false>(r.mb));
        if(r.ma > 0)
            pp.A.add(torder::marketorder<true>(r.ma));
        if(r.b.volume)
        {
            pp.B.add(r.b);
            er.all = true;
        }
        if(r.a.volume)
        {
            pp.A.add(r.a);
            er.all = true;
        }
        return trequest(pp,er,finterval);
    }
    ttime finterval;
};

} // namespace

#endif // MARKETSIM_HPP
