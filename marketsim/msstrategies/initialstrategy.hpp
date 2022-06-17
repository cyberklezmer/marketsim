#ifndef INITIALSTRATEGY_HPP
#define INITIALSTRATEGY_HPP

#include "marketsim.hpp"

namespace marketsim
{

class generalinitialstrategy : public teventdrivenstrategy
{
public:
    generalinitialstrategy(tprice b, tprice a, tvolume v)
        : teventdrivenstrategy(0), fb(b), fa(a), fv(v) {}
    virtual trequest event(const tmarketinfo& info, tabstime t, trequestresult*)
    {
        const twallet& w = info.mywallet();
        assert(w.stocks() >= fv);
        assert(w.money() >= fv * fb);

        tpreorderprofile pp;

        tpreorder p(fb,fv);
        pp.B.add(p);
        tpreorder o(fa,fv);
        pp.A.add(o);
        setinterval(std::numeric_limits<tabstime>::max());
        return trequest({pp,trequest::teraserequest(false),0});
    }
private:
    tprice fb;
    tprice fa;
    tvolume fv;
};

template <int b, int a, int v = 1>
class initialstrategy : public generalinitialstrategy
{
public:
    initialstrategy() : generalinitialstrategy(b, a, v)
    {
        static_assert(a>b);
    }
};

} // namespace

#endif // INITIALSTRATEGY_HPP
