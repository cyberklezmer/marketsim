#ifndef PETER_HPP
#define PETER_HPP

#include "strategies.hpp"
#include<math.h>
#include<fstream>
#include<boost/random/mersenne_twister.hpp>
#include<boost/random/uniform_real.hpp>
#include<boost/random/discrete_distribution.hpp>

using TSpace = std::vector<std::vector<std::vector<double>>>;
using TState = std::vector<std::vector<double>>;
using TSpace_d = std::vector<std::vector<std::vector<int>>>;
using TState_d = std::vector<std::vector<int>>;

boost::random::mt19937 gen;

int myPow(int x, unsigned int p)
{
    if (p == 0) return 1;
    if (p == 1) return x;

    int tmp = myPow(x, p / 2);
    if (p % 2 == 0) return tmp * tmp;
    else return x * tmp * tmp;
}

void normalize(TState& M)
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

void writeMatrix(TState M, std::string filename)
{
    std::ofstream file;
    file.open(filename);
    for (unsigned i = 0; i < M.size(); i++)
    {
        for (unsigned j = 0; j < M[0].size(); j++)
            file << M[i][j] << " ";
        file << "\n";
    }
}

TState readMatrix(std::string filename)
{
    TState res;
    std::ifstream file;
    file.open(filename);

    std::string line;
    while (std::getline(file, line))
    {
        res.push_back(std::vector<double>());
        std::vector<double>& row = res.back();
        std::istringstream iss(line);
        double value;
        while (iss >> value)
            row.push_back(value);
    }
    file.close();
    return res;
}

int shift(int h, int p, unsigned max)
{
    int tmp = 1;
    if (p < 0) tmp = 0;
    else if (p > 0) tmp = 2;

    std::vector<int> ht;
    if (h == 0)
        ht.push_back(0);
    else
    {
        for (int r = h; r > 0; r /= 3)
            ht.push_back(r % 3);
    }

    while (ht.size() < max)
        ht.push_back(0);

    std::reverse(ht.begin(), ht.end());
    ht.erase(ht.begin());
    ht.push_back(tmp);

    int index = 0;
    for (unsigned i = 0; i < ht.size(); i++)
        index += myPow(3, i) * ht[ht.size() - 1 - i];

    return index;
}

//int sample(std::vector<double> distr)
//{
//	std::vector<double> cd;
//	cd.push_back(0.0); //cumulative distribution
//	for (unsigned i = 0; i < distr.size(); i++)
//	{
//		cd.push_back(cd.back() + distr[i]);
//	}
//
//	boost::random::uniform_real_distribution<double> d(0, 1);
//	double p = d(gen);
//
//	for (unsigned j = 0; j < cd.size() - 1; j++)
//	{
//		if (p > cd[j] && p < cd[j + 1])
//			return -static_cast<int>(distr.size() - 1) / 2 + j;
//	}
//}

TSpace_d adp_getPolicy(const unsigned iter, const unsigned time, const unsigned max_stocks, const unsigned max_hist, TState P)
{
    double bigM = 1e6;
    TSpace V(time, TState(myPow(3, max_hist), std::vector<double>(max_stocks + 1, bigM)));
    V.push_back(TState(myPow(3, max_hist), std::vector<double>(max_stocks + 1, 0))); //V_T = 0
    TSpace_d A(time + 1, TState_d(myPow(3, max_hist), std::vector<int>(max_stocks + 1, 0)));

    int q = (P[0].size() - 1) / 2;
    std::vector<double> probs(2 * q + 1, 1.0 / (2 * q + 1));

    unsigned h = (myPow(3, max_hist) - 1) / 2; //initial index of history
    unsigned x = max_stocks / 2; //initial number of stocks

    for(unsigned n = 1; n <= iter; n++)
    {
        for (unsigned t = 0; t < time; t++)
        {
            boost::random::discrete_distribution<> dist(P[h]);
            int incr = dist(gen) - q; //generates a price increment from a known distribution

            std::vector<double> tmp; //stores possible values for the state depending on decision
            int bnd_l = -1, bnd_u = 1;
            if (x == 0) bnd_l = 0;
            else if (x == max_stocks) bnd_u = 0; //to make sure only possible decisions are considered at x = 0 or x = max_stocks

            for (int a = bnd_l; a <= bnd_u; a++) //iterate through possible decisions
            {
                double val = 0.0; //stores the expected value
                for (int p = -q; p <= q; p++)
                {
                    val += ((x + a) * p + V[t + 1][shift(h, p, max_hist)][x + a]) * P[h][q + p]; //Bellman eqn
                }
                tmp.push_back(val);
            }

            V[t][h][x] = *std::max_element(tmp.begin(), tmp.end()); //update value for the state we are currently in
            int d = std::distance(tmp.begin(), std::max_element(tmp.begin(), tmp.end())); //index of the max value
            int opt_a = (x == 0) ? d : d - 1; //optimal decision (ternary op. is necessary so this can work with any tmp - can be of size 2 or 3)

            A[t][h][x] = opt_a; //remember optimal decision
            x += opt_a; //we choose the optimal decision and increment the number of stocks accordingly
            h = shift(h, incr, max_hist); //history is updated depending on the price shift we generated
        }
    }
    return A;
}

namespace marketsim
{
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
            if (mi.t > timeinterval() && mi.t < 2*timeinterval())
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

    class adp_trend : public tsimplestrategy
    {
    public:
        adp_trend(const std::string& name, tvolume delta, ttime timeinterval, const int _max_stocks, const int _max_hist, TSpace_d _P)
            : tsimplestrategy(name, timeinterval), max_stocks(_max_stocks), max_hist(_max_hist), fdelta(delta), P(_P)
        {}
    private:
        virtual tsimpleorderprofile simpleevent(const tmarketinfo& mi, const ttradinghistory& th)
        {
            if(mi.t > max_hist*timeinterval())
            {
                std::vector<double> hist;
                for (int i = 0; i < max_hist; i++)
                {
                    double p = mi.history.p(mi.t - i * timeinterval()) - mi.history.p(mi.t - (i + 1) * timeinterval());
                    if (abs(p) > std::numeric_limits<double>::epsilon())
                        hist.push_back(p < 0 ? 0 : 2);
                    else
                        hist.push_back(1);
                }
                std::reverse(hist.begin(), hist.end());

                int x = th.wallet().stocks();
                int index = 0;
                for (unsigned i = 0; i < hist.size(); i++)
                    index += myPow(3, i) * hist[hist.size() - 1 - i];
                int t = int(mi.t);
                if (P[t][index][x])
                    return P[t][index][x] == 1 ? buymarket(fdelta) : sellmarket(fdelta);
            }
            return noorder();
        }
        tvolume fdelta;
        TSpace_d P;
        const int max_hist, max_stocks;
    };

    TState getTransitionMatrix(const unsigned iter, const unsigned T, const unsigned m, const int q)
    {
        TState P(myPow(3, m), std::vector<double>(2 * q + 1, 0));

        tstollmarketmaker mm("mm", 500, 5000, 1, 0.01);
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
                    index += myPow(3, i) * h[h.size() - 1 - i];

                int incr = lround(tsimulation::results().history.p(j + m + 1) - tsimulation::results().history.p(j + m));
                if (incr > q) incr = q;
                else if (incr < -q) incr = -q;
                P[index][q+incr]++;
            }
        }
        normalize(P);
        return P;
    }

} // namespace
#endif // PETER_HPP
