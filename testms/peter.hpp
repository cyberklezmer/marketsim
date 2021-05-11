#ifndef PETER_HPP
#define PETER_HPP

#include "strategies.hpp"

namespace marketsim
{
    class ta_macd : public tsimplestrategy
    {
    public:
        ta_macd(const std::string& name, tvolume delta, ttime timeinterval, const int _s, const int _f, const int _sgn)
            : tsimplestrategy(name, timeinterval), fdelta(delta), s(_s), f(_f), sgn(_sgn)
        {}
        const int f, s, sgn;
        const double
            alpha_f = 2.0 / (f + 1.0), beta_f = 1 - alpha_f,
            alpha_s = 2.0 / (s + 1.0), beta_s = 1 - alpha_s,
            alpha_sgn = 2.0 / (sgn + 1.0), beta_sgn = 1 - alpha_sgn;
        std::vector<double> ema_slow, ema_fast, macd, signal;
    private:
        virtual tsimpleorderprofile simpleevent(const tmarketinfo& mi,
            const ttradinghistory&)
        {
            double p = mi.history.p(mi.t);
            if(mi.t > timeinterval() && mi.t < 2*timeinterval())
            {
                ema_slow.push_back(p);
                ema_fast.push_back(p);
                macd.push_back(0);
                signal.push_back(0);
            }
            else if (!isnan(p))
            {
                ema_slow.push_back(alpha_s * p + beta_s * ema_slow.back());
                ema_fast.push_back(alpha_f * p + beta_f * ema_fast.back());
                macd.push_back(ema_slow.back() - ema_fast.back());
                signal.push_back(alpha_sgn * macd.back() + beta_sgn * signal.back());

                if (macd.end()[-2] < signal.end()[-2] && macd.back() > signal.back())
                    return buymarket(fdelta);
                else if (macd.end()[-2] > signal.end()[-2] && macd.back() < signal.back())
                    return sellmarket(fdelta);
            }
            return noorder();
        }
        tvolume fdelta;
    };
} // namespace


#endif // PETER_HPP
