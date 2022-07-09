#ifndef CANCELLINGDSSTRATEGY_HPP
#define CANCELLINGDSSTRATEGY_HPP

#include "marketsim.hpp"

namespace marketsim
{

template <typename T, int meanlife>
class cancellingstrategy: public T
{
    bool eraseit(tabstime dt)
    {
        return -log(T::uniform()) * meanlife < dt;
    }


public:
    cancellingstrategy() : flastcancellation(0) {}

    virtual trequest event(const tmarketinfo& info, tabstime t, trequestresult* resultoflast)
    {
        const auto& p = info.myorderprofile();


        trequest r = T::event(info,t,resultoflast);
        if(!r.eraserequest().all)
        {
            double dt = t-flastcancellation;
            trequest::teraserequest oldr = r.eraserequest();
            trequest::teraserequest newr(false);
            for(unsigned i=0; i<p.A.size(); i++)
            {
                bool doit = eraseit(dt);
                newr.a.push_back(doit || (i < oldr.a.size() && oldr.a[i] == true));
            }
            for(unsigned i=0; i<p.B.size(); i++)
            {
                bool doit = eraseit(dt);
                newr.b.push_back(doit || (i < oldr.b.size() && oldr.b[i] == true));
            }
            r.seteraserequest(newr);
        }
        flastcancellation = t;
        return r;
    }

private:
   tabstime flastcancellation;
};

} // namespace

#endif // CANCELLINGDSSTRATEGY_HPP
