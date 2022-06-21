#ifndef SPECULATOR_HPP
#define SPECULATOR_HPP

#include "marketsim.hpp"
#include <numeric>
namespace marketsim
{

	///A speculative strategy based on approximate dynamic programming

	class speculator : public teventdrivenstrategy
	{
		using Tvec = std::vector<double>;
		using T2vec = std::vector<Tvec>;
		using T3vec = std::vector<T2vec>;
		using T4vec = std::vector<T3vec>;

	public:
		speculator()
			: teventdrivenstrategy(1)
		{
			finitprice = 100; //TBD change this to something more dynamic
			fofferedvolume = 1;
			fbndmoney = 0, fbndstocks = 0;
			fldelta = 100, fudelta = 200;
			W = T2vec(fbndmoney + 1, Tvec(fbndstocks + 1, 0.0));
			N = P = T4vec(2, T3vec(2, T2vec(fldelta + fudelta + 1, Tvec(fldelta + fudelta + 1, 0.0))));
			last_bid = klundefprice, last_ask = khundefprice;
			Kparam = 3000, fepsparam = 0.2, fdiscfact = 0.9998;
			n = 0;
		}
	private:
		virtual trequest event(const tmarketinfo& mi, tabstime t, trequestresult* lastresult)
		{
			bool firsttime = lastresult == 0;
			//initializations
			tprice alpha = mi.alpha();
			tprice beta = mi.beta();
			double p = (alpha != khundefprice && beta != klundefprice)
				? (alpha + beta) / 2 : finitprice;
			if (firsttime)
			{
				//initialize W
				fbndmoney = mi.mywallet().money() * 10;
				fbndstocks = mi.mywallet().stocks() + 100;
				for (int i = 0; i <= fbndmoney; i++)
					for (int j = 0; j <= fbndstocks; j++)
					{ 
						W[i][j] = (i == 0 && i == j) ? 0 : (1.0 - pow(fdiscfact, i + j * finitprice)) / (1.0 - fdiscfact);
						//W[i][j] = i + j * finitprice;
					}
				//initialize N
				countKparam = Kparam;
				for (int a = 0; a <= fldelta + fudelta; a++)
				{
					for (int b = 0; b <= fldelta + fudelta; b++)
					{
						double pra = (a < fudelta) ? 0.25 : ((a > fudelta) ? 0.0 : 1.0 / 3);
						double prb = (b > fldelta) ? 0.25 : ((b < fldelta) ? 0.0 : 1.0 / 3);
						N[1][1][a][b] = 0;
						N[1][0][a][b] = Kparam * pra;
						N[0][1][a][b] = Kparam * prb;
						N[0][0][a][b] = Kparam - N[1][0][a][b] - N[0][1][a][b];
					}
				}
				n = mi.mywallet().stocks();
			}


			//update alpha, beta
			if (beta == klundefprice) beta = (last_bid == klundefprice) ? p - 1 : last_bid;
			if (alpha == khundefprice) alpha = (last_ask == khundefprice) ? p + 1 : last_ask;

			tprice da = std::min(std::max(mi.history().a(t) - alpha, beta - alpha - fldelta), beta - alpha + fudelta);
			tprice db = std::min(std::max(mi.history().b(t) - beta, alpha - beta - fudelta), alpha - beta + fldelta);

			//update information (c,d) about the last action
			int c = 0, d = 0;
			if (mi.mywallet().stocks() < n) { c = 1; n--; }
			if (mi.mywallet().stocks() > n) { d = 1; n++; }

			//update and normalize N
			N[c][d][da - (beta - alpha - fldelta)][db - (alpha - beta - fudelta)]++;
			countKparam++;

			for (int a = 0; a <= fldelta + fudelta; a++)
				for (int b = 0; b <= fldelta + fudelta; b++)
					for (unsigned i = 0; i < N.size(); i++)
						for (unsigned j = 0; j < N[0].size(); j++)
							P[i][j][a][b] = N[i][j][a][b] / countKparam;

			//calculate value function
			double v = 0.0;
			tprice m = mi.mywallet().money(); int s = mi.mywallet().stocks();
			tprice a_best = p + 1; tprice b_best = std::min(double(m), p - 1); tprice cash_best = 0;
			for (tprice cash = 0; m - b_best - cash >= 0; cash++)
				for (tprice b = alpha; (b > 0) && (m - cash - b >= 0) && (b >= alpha - fudelta); b--)
					for (tprice a = beta; (a > b) && (a <= beta + fudelta); a++)
					{
						tprice da = std::min(std::max(a - alpha, beta - alpha - fldelta), beta - alpha + fudelta);
						tprice db = std::min(std::max(b - beta, alpha - beta - fudelta), alpha - beta + fldelta);
						tprice da_best = std::min(std::max(a_best - alpha, beta - alpha - fldelta), beta - alpha + fudelta);
						tprice db_best = std::min(std::max(b_best - beta, alpha - beta - fudelta), alpha - beta + fldelta);

						double u = 0.0, u_best = 0.0;
						for (int C = 0; C <= 1; C++)
							for (int D = 0; (D <= 1) && (s + D - C >= 0); D++)
							{
								u_best += cash_best + fdiscfact * W[m - cash_best - D * b_best + C * a_best][s + D - C]
									* P[C][D][da_best - (beta - alpha - fldelta)][db_best - (alpha - beta - fudelta)];
								u += cash + fdiscfact * W[m - cash - D * b + C * a][s + D - C]
									* P[C][D][da - (beta - alpha - fldelta)][db - (alpha - beta - fudelta)];
							}
						if (u_best < u)
						{
							a_best = a; b_best = b; cash_best = cash;
							v = u;
						}
					}

			b_best = m > 0 ? b_best : klundefprice;
			a_best = n > 0 ? a_best : khundefprice;
			last_ask = a_best;
			last_bid = b_best;

			//projection operator
			double w = fepsparam * v + (1.0 - fepsparam) * W[m][s];
			for (int m_ind = 0; m_ind < W.size(); m_ind++)
				W[m_ind][n] = (m_ind < m) ? std::min(W[m_ind][n], w) : std::max(W[m_ind][n], w);
			for (int s_ind = 0; s_ind < W[0].size(); s_ind++)
				W[m][s_ind] = (s_ind < n) ? std::min(W[m][s_ind], w) : std::max(W[m][s_ind], w);

			trequest ord;
			ord.addbuylimit(b_best, fofferedvolume);
			ord.addselllimit(a_best, fofferedvolume);
			ord.setconsumption(cash_best);
			return ord;
		}

		tprice finitprice;
		tvolume fofferedvolume;
		double
			fdiscfact,
			fepsparam;
		int Kparam, countKparam,
			fbndmoney, fbndstocks,
			fldelta,
			fudelta;
		int n;
		T2vec W;
		T4vec N, P;
		tprice last_bid, last_ask;
	};

} // namespace


#endif // SPECULATOR_HPP
