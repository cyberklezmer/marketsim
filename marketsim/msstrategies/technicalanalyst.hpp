#include "marketsim.hpp"
#include<vector>
#include<numeric>
namespace marketsim
{
    class ta_macd : public teventdrivenstrategy
    {
    public:
        ta_macd() : teventdrivenstrategy(1)
        {
            len_short = 12, len_long = 26, len_sgn = 9;
            step = 0;
        }

    private:
        virtual trequest event(const tmarketinfo& mi, tabstime, trequestresult*)
        {
            tprice alpha = mi.alpha(), beta = mi.beta();
            double p = (alpha != khundefprice && beta != klundefprice) ?
                (alpha + beta) / 2 : std::numeric_limits<double>::quiet_NaN();
            trequest req;

            if (!isnan(p))
            {

                if (step <= len_long) prices_head.push_back(p);

                if (step == len_short) ema_short_curr = sma(prices_head);
                else if (step > len_short)
                {
                    ema_short_prev = ema_short_curr;
                    ema_short_curr = ema(ema_short_curr, p, len_short);
                }

                if (step == len_long) ema_long_curr = sma(prices_head);
                else if (step > len_long)
                {
                    ema_long_prev = ema_long_curr;
                    ema_long_curr = ema(ema_long_curr, p, len_long);
                    macd_prev = ema_short_prev - ema_long_prev;
                    macd_curr = ema_short_curr - ema_long_curr;
                    if (step <= len_long + len_sgn) signal_head.push_back(macd_curr);
                }

                if (step == len_long + len_sgn) signal_curr = sma(signal_head);
                else if (step > len_long + len_sgn)
                {
                    signal_prev = signal_curr;
                    signal_curr = ema(signal_curr, p, len_sgn);
                }

                if (macd_prev < signal_prev && macd_curr > signal_curr)
                    req.addbuymarket(fdelta);
                if (macd_prev > signal_prev && macd_curr < signal_curr)
                    req.addsellmarket(fdelta);

                step++;
            }

            return req;
        }

        inline double sma(std::vector<double> prices)
        {
            return std::accumulate(prices.begin(), prices.end(), 0.0) / prices.size();
        }

        inline double ema(double ema_prev, double price_curr, int len)
        {
            return 2.0 / (len + 1) * price_curr + ema_prev * (1 - 2.0 / (len + 1));
        }

        tvolume fdelta;
        int step;
        int len_short, len_long, len_sgn;
        double ema_short_curr, ema_short_prev,
            ema_long_curr, ema_long_prev,
            macd_curr, macd_prev,
            signal_curr, signal_prev;
        std::vector<double> prices_head, signal_head;
    };
}