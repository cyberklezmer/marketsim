#ifndef RAWDS_HPP
#define RAWDS_HPP

#include "marketsim.hpp"

namespace marketsim
{

class generalrawds : public tdsbase
{
public:
    generalrawds(int eventsperhour, int volumemean) : lastt(0),
        feventsperhour(eventsperhour), fvolumemean(volumemean),
        pd(volumemean)
    {}
    virtual tdsrecord delta(tabstime t, const tmarketdata& md)
    {
        tdsrecord ret = {0,0};
        if(t > lastt)
        {
            double lambda = feventsperhour / 3600.0;
            assert(lambda > 0);
            double u = uniform();
            if(-log(u)/lambda < (t-lastt))
            {
                bool buy = uniform() > 0.5;
                auto volume = pd(engine());
                if(volume)
                {
                    if(buy)
                    {
                        auto a = md.lastdefineda();
                        if(a != khundefprice)
                        {
                            ret = { volume * a,0 };
                        }
                    }
                    else
                    {
                        auto b = md.lastdefinedb();
                        if(b != klundefprice)
                        {
                            ret = { 0,volume };
                        }

                    }
                }
            }
            lastt = t;
        }
        return ret;
    }
private:
    tabstime lastt;
    std::poisson_distribution<> pd;
    int feventsperhour;
    int fvolumemean;
};

template <int eventsperhour, int volumemean>
class rawds : public generalrawds
{
public:
    rawds() : generalrawds(eventsperhour,volumemean)  {}
};


} // namespace
#endif // RAWDS_HPP
