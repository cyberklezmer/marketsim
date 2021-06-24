#ifndef PETER_HPP
#define PETER_HPP

#include "strategies.hpp"
#include<math.h>
#include<fstream>

void normalize(std::vector<std::vector<double>>& M)
{
    for (unsigned i = 0; i < M.size(); i++)
    {
        double sum = 0.0;
        for (unsigned j = 0; j < M[0].size(); j++) sum += M[i][j];
        if (sum > 0)
        {
            for (unsigned j = 0; j < M[0].size(); j++) M[i][j] /= sum;
        }
    }
}

int myPow(int x, unsigned int p)
{
    if (p == 0) return 1;
    if (p == 1) return x;

    int tmp = myPow(x, p / 2);
    if (p % 2 == 0) return tmp * tmp;
    else return x * tmp * tmp;
}

namespace marketsim
{
    class adp : public tsimplestrategy
    {
    public:
        adp(const std::string& name, tvolume delta, ttime timeinterval, tvolume _max_stocks, const int _max_hist)
            : tsimplestrategy(name, timeinterval), fdelta(delta), max_stocks(_max_stocks), max_hist(_max_hist)
        {}
    private:
        virtual tsimpleorderprofile simpleevent(const tmarketinfo& mi, const ttradinghistory&)
        {

        }
        tvolume fdelta, max_stocks;
        const int max_hist;
    };

    class ta_macd : public tsimplestrategy
    {
    public:
        ta_macd(const std::string& name, tvolume delta, ttime timeinterval, const int _s, const int _f, const int _sgn)
            : tsimplestrategy(name, timeinterval), fdelta(delta), s(_s), f(_f), sgn(_sgn)
        {}
    private:
        virtual tsimpleorderprofile simpleevent(const tmarketinfo& mi, const ttradinghistory&)
        {
            double p = mi.history.p(mi.t);
            if (mi.t > timeinterval() && mi.t < 2 * timeinterval())
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

                if (mi.t > s)
                {
                    if (macd.end()[-2] < signal.end()[-2] && macd.back() > signal.back())
                        return buymarket(fdelta);
                    else if (macd.end()[-2] > signal.end()[-2] && macd.back() < signal.back())
                        return sellmarket(fdelta);
                }
            }
            return noorder();
        }
        tvolume fdelta;
        const int f, s, sgn;
        const double
            alpha_f = 2.0 / (f + 1.0), beta_f = 1 - alpha_f,
            alpha_s = 2.0 / (s + 1.0), beta_s = 1 - alpha_s,
            alpha_sgn = 2.0 / (sgn + 1.0), beta_sgn = 1 - alpha_sgn;
        std::vector<double> ema_slow, ema_fast, macd, signal;
    };

    std::vector<std::vector<double>> getTransitionMatrix(const int m, const int T, const int q, const int iter)
    {
        std::vector<std::vector<double>> P(myPow(3, m), std::vector<double>(2 * q + 1, 0));

        tstollmarketmaker mm("mm", 500, 5000, 1);
        tliquiditytaker lt("lt", 1, 1);
        ta_macd macd("macd", 10, 1, 26, 12, 9);

        tsimulation::addstrategy(mm, { 100000,5000 });
        tsimulation::addstrategy(lt, { 100000,5000 });
        tsimulation::addstrategy(macd, { 100000,5000 });

        for (unsigned i = 0; i < iter; i++)
        {
            tsimulation::run(T);

            for (unsigned j = 1; j < T - m; j++)
            {
                std::vector<int> h;
                for (unsigned k = j; k < j + m; k++)
                {
                    double p_n = tsimulation::results().history.p(k + 1);
                    double p_o = tsimulation::results().history.p(k);
                    if (abs(p_n - p_o) > std::numeric_limits<double>::epsilon())
                        h.push_back(p_n > p_o ? 2 : 0);
                    else
                        h.push_back(1);
                }
                unsigned index = 0;
                for (unsigned i = 0; i < h.size(); i++)
                    index += myPow(3, i) * h[i];

                int incr = lround(tsimulation::results().history.p(j + m + 1) - tsimulation::results().history.p(j + m));
                if (incr > q) incr = q;
                else if (incr < -q) incr = -q;
                P[index][q+incr]++;
            }
        }
        normalize(P);
        return P;
    }

    void writeMatrix(std::vector<std::vector<double>> M)
    {
        std::ofstream file;
        file.open("matrices.csv");
        file << "\n\n" << ",";

        for (int i = 0; i < M[0].size(); i++) file << -0.5 * (M[0].size() - 1) + i <<",";
        for (int i = 0; i < M.size(); i++)
        {
            file << i << ",";
            for (int j = 0; j < M[0].size(); j++)
                file << M[i][j] << ",";
            file << "\n";
        }
    }

} // namespace

#endif // PETER_HPP

