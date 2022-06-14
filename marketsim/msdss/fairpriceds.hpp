#ifndef FAIRPRICEDS_HPP
#define FAIRPRICEDS_HPP

#include "marketsim.hpp"

namespace marketsim
{

template <int eventsperhour, int volumemean, int volatilityperc = 30>
class fairpriceds : public tdsbase
{
public:
    fairpriceds() : lastt(0), pd(volumemean)
    {}
    virtual tdsrecord delta(tabstime t, const tmarketdata& md)
    {
        tdsrecord ret = {0,0};
        if(t > lastt )
        {
            constexpr double lambda = eventsperhour / 3600.0;
            static_assert(lambda > 0);
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
};

} // namespace

#endif // FAIRPRICEDS_HPP
