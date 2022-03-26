#ifndef TADPMARKETMAKER_HPP
#define TADPMARKETMAKER_HPP

#include "marketsim.hpp"

namespace marketsim
{

	///  A market making strategy based on approximate dynamic programming

	class tadpmarketmaker : public teventdrivenstrategy
	{
		using Tvec = std::vector<double>;
		using T2vec = std::vector<Tvec>;
		using T3vec = std::vector<T2vec>;
		using T4vec = std::vector<T3vec>;

	public:
		tadpmarketmaker()
			: teventdrivenstrategy(1),
			finitprice(100),
			fbndmoney(0), fbndstocks(0),
			fofferedvolume(1),
			fldelta(25), fudelta(40),
			fdiscfact(0.9998), fepsparam(0.2), fKparam(3000),
			m(0), n(0),
			W(T2vec(0, Tvec(0, 0.0))),
			N(T4vec(2, T3vec(2, T2vec(fldelta + fudelta + 1, Tvec(fldelta + fudelta + 1, 0.0))))),
			P(T4vec(2, T3vec(2, T2vec(fldelta + fudelta + 1, Tvec(fldelta + fudelta + 1, 0.0))))),
			last_bid(klundefprice), last_ask(khundefprice)
		{

		}
	private:
		virtual trequest event(const tmarketinfo& mi, tabstime t, bool firsttime)
		{
			//initializations

			tprice alpha = mi.alpha();
			tprice beta = mi.beta();
			double p = alpha != khundefprice && beta != klundefprice
				? (alpha + beta) / 2 : finitprice;

			if (firsttime)
			{
				//initialize W
				fbndmoney = mi.mywallet().money() * 100;
				fbndstocks = mi.mywallet().stocks() + 100;
				for (int i = 0; i <= fbndmoney; i++)
				{
					W.push_back(std::vector<double>(fbndstocks + 1, 0.0));
					for (int j = 0; j <= fbndstocks; j++)
						W[i][j] = (i == 0 && i == j) ? 0 : (1.0 - pow(fdiscfact, i + j * finitprice)) / (1.0 - fdiscfact);
				}
				//initialize N
				for (int a = 0; a <= fldelta + fudelta; a++)
				{
					for (int b = 0; b <= fldelta + fudelta; b++)
					{
						double pra = (a < fudelta) ? 0.48 : ((a > fudelta) ? 0.0 : 1.0 / 3);
						double prb = (b > fldelta) ? 0.48 : ((b < fldelta) ? 0.0 : 1.0 / 3);
						N[1][1][a][b] = 0;
						N[1][0][a][b] = fKparam * pra;
						N[0][1][a][b] = fKparam * prb;
						N[0][0][a][b] = fKparam - N[1][0][a][b] - N[0][1][a][b];
					}
				}
				m = mi.mywallet().money();
				n = mi.mywallet().stocks();
			}



			//update bid and ask (alpha, beta)

			if (beta == klundefprice) beta = (last_bid == klundefprice) ? p - 1 : last_bid;
			if (alpha == khundefprice) alpha = (last_ask == khundefprice) ? p + 1 : last_ask;

			//differences da, db; cut off according to bounds \fudelta, \fldelta
			tprice da = std::min(std::max(mi.history.a(t) - alpha, beta - alpha - fldelta), beta - alpha + fudelta);
			tprice db = std::min(std::max(mi.history.b(t) - beta, alpha - beta - fudelta), alpha - beta + fldelta);

			//update information (C,D) about the action of the market maker
			int d = 0, c = 0;
			if (mi.mywallet().stocks() < n) { c = 1; n--; }
			else if (mi.mywallet().stocks() > n) { d = 1; n++; }

			//update and normalize N to a probability measure
			N[c][d][da - (beta - alpha - fldelta)][db - (alpha - beta - fudelta)]++;

			for (int a = 0; a <= fudelta + fldelta; a++)
				for (int b = 0; b <= fudelta + fldelta; b++)
				{
					double sum = 0.0;
					for (int i = 0; i < N.size(); i++)
						for (int j = 0; j < N[0].size(); j++)
							sum += N[i][j][a][b];

					if (sum > 0)
					{
						for (int i = 0; i < N.size(); i++)
							for (int j = 0; j < N[0].size(); j++)
								P[i][j][a][b] = N[i][j][a][b] / sum;
					}
				}

			//calculate v as the expected value
			double v = 0.0;
			tprice money = mi.mywallet().money(); int s = mi.mywallet().stocks();
			tprice a_best = alpha; tprice b_best = (money - beta >= 0) ? beta : money; tprice cash_best = 0;

			for (tprice cash = 0; cash <= 1; cash++)
				for (tprice b = alpha - fudelta; (b > 0) && (b <= alpha + fldelta) && (money - cash - b >= 0); b++)
					for (tprice a = beta - fldelta; (a > b) && (a <= beta + fudelta) && (money - cash - b + a < fbndmoney); a++)
					{
						tprice da = std::min(std::max(a - alpha, beta - alpha - fldelta), beta - alpha + fudelta);
						tprice db = std::min(std::max(b - beta, alpha - beta - fudelta), alpha - beta + fldelta);
						tprice da_best = std::min(std::max(a_best - alpha, beta - alpha - fldelta), beta - alpha + fudelta);
						tprice db_best = std::min(std::max(b_best - beta, alpha - beta - fudelta), alpha - beta + fldelta);

						double u = 0.0, u_best = 0.0;
						for (int C = 0; C <= 1; C++)
							for (int D = 0; (D <= 1) && (s + D - C >= 0) && (s + D - C < fbndstocks); D++)
							{
								u_best += cash_best + fdiscfact * W[money - cash_best - D * b_best + C * a_best][s + D - C]
									* P[C][D][da_best - (beta - alpha - fldelta)][db_best - (alpha - beta - fudelta)];
								u += cash + fdiscfact * W[money - cash - D * b + C * a][s + D - C]
									* P[C][D][da - (beta - alpha - fldelta)][db - (alpha - beta - fudelta)];
							}
						if (u_best < u)
						{
							a_best = a; b_best = b; cash_best = cash;
							v = u;
						}
					}

			last_ask = a_best;
			last_bid = b_best;

			//projection operator
			double w = fepsparam * v + (1.0 - fepsparam) * W[m][n];
			for (int mm = 0; mm < W.size(); mm++)
				W[mm][n] = (mm < m) ? std::min(W[mm][n], w) : std::max(W[mm][n], w);
			for (int nn = 0; nn < W[0].size(); nn++)
				W[m][nn] = (nn < n) ? std::min(W[m][nn], w) : std::max(W[m][nn], w);

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
		int fKparam,
			fbndmoney,
			fbndstocks,
			fldelta,
			fudelta;
		int m, n;
		T2vec W;
		T4vec N, P;
		tprice last_bid, last_ask;
	};

} // namespace


#endif // TADPMARKETMAKER_HPP
