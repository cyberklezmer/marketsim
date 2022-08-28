#ifndef ADPMARKETMAKER_HPP
#define ADPMARKETMAKER_HPP

#include "marketsim.hpp"
#include<fstream>
#include<algorithm>

namespace marketsim
{

	///  A market making strategy based on approximate dynamic programming

	class adpmarketmaker : public teventdrivenstrategy
	{
		using Tvec = std::vector<double>;
		using T2vec = std::vector<Tvec>;
		using T3vec = std::vector<T2vec>;
		using T4vec = std::vector<T3vec>;

	public:
		adpmarketmaker() : teventdrivenstrategy(1)
		{
			initprice = 100, qvol = 1;
			last_stocks = 0;
			ldelta = 20, udelta = 40;
			N = P = T4vec(2, T3vec(2, T2vec(ldelta + udelta + 1, Tvec(ldelta + udelta + 1, 0.0))));
			last_bid = klundefprice, last_ask = khundefprice;
			Kparam = 3000, epsparam = 0.2, discfact = 0.99998;
		}

	private:
        virtual trequest event(const tmarketinfo& mi, tabstime t, trequestresult* lastresult)
		{
            bool firsttime = lastresult == 0;

			tprice alpha = mi.alpha();
			tprice beta = mi.beta();
			double p = (alpha != khundefprice && beta != klundefprice)
				? (alpha + beta) / 2 : initprice;
			if (firsttime)
			{
				//initialize W
				bndmoney = mi.mywallet().money() * 10;
				bndstocks = mi.mywallet().stocks() + 100;
				W = T2vec(bndmoney + 1, Tvec(bndstocks + 1, 0.0));
				for (int i = 0; i <= bndmoney; i++)
				{
					for (int j = 0; j <= bndstocks; j++)
					{ 
						W[i][j] = (i == 0 && i == j) ? 0 : (1.0 - pow(discfact, i + j * initprice)) / (1.0 - discfact);
					}
				}
				//initialize N
				countKparam = Kparam;
				for (int a = 0; a <= ldelta + udelta; a++)
				{
					for (int b = 0; b <= ldelta + udelta; b++)
					{
						double pra = (a < udelta) ? 0.25 : ((a > udelta) ? 0.0 : 1.0 / 3);
						double prb = (b > ldelta) ? 0.25 : ((b < ldelta) ? 0.0 : 1.0 / 3);
						N[1][1][a][b] = 0;
						N[1][0][a][b] = Kparam * pra;
						N[0][1][a][b] = Kparam * prb;
						N[0][0][a][b] = Kparam - N[1][0][a][b] - N[0][1][a][b];
					}
				}
				last_stocks = mi.mywallet().stocks();
			}


			//update alpha, beta
			if (beta == klundefprice) beta = (last_bid == klundefprice) ? p - 1 : last_bid;
			if (alpha == khundefprice) alpha = (last_ask == khundefprice) ? p + 1 : last_ask;

            tprice da = std::min(std::max(mi.history().a(t) - alpha, beta - alpha - ldelta), beta - alpha + udelta);
            tprice db = std::min(std::max(mi.history().b(t) - beta, alpha - beta - udelta), alpha - beta + ldelta);

			//update information (c,d) about the last action of the market maker
			int c = 0, d = 0, diff = mi.mywallet().stocks() - last_stocks;
			if (diff > 0) c = 1;
			if (diff < 0) d = 1;
			last_stocks += diff;

			//update and normalize N
			N[c][d][da - (beta - alpha - ldelta)][db - (alpha - beta - udelta)]++;
			countKparam++;

			for (int a = 0; a <= ldelta + udelta; a++)
				for (int b = 0; b <= ldelta + udelta; b++)
					for (int i = 0; i <=  1; i++)
						for (int j = 0; j <= 1; j++)
							P[i][j][a][b] = N[i][j][a][b] / countKparam;

			//calculate value function
			double v = 0.0;
			tprice m = mi.mywallet().money(); tvolume s = mi.mywallet().stocks();
			tprice a_best = p + 1, b_best = std::min<tprice>(m, p - 1);
			tprice cons = 0;
			if (m > 5 * p) cons = m - 5 * p;

			for (tprice b = std::max(alpha - udelta, 1); (b < alpha) && (m - cons - std::max(b,b_best) * qvol >= 0); b++)
				for (tprice a = beta + udelta; (a > b) && (a > beta); a--)
				{
					tprice da = std::min(std::max(a - alpha, beta - alpha - ldelta), beta - alpha + udelta),
						db = std::min(std::max(b - beta, alpha - beta - udelta), alpha - beta + ldelta),
						da_best = std::min(std::max(a_best - alpha, beta - alpha - ldelta), beta - alpha + udelta),
						db_best = std::min(std::max(b_best - beta, alpha - beta - udelta), alpha - beta + ldelta);

					double u = 0.0, u_best = 0.0;
					for (int C = 0; C <= 1 && (s - C * qvol >= 0); C++)
						for (int D = 0; (D <= 1); D++)
						{
							int C_hat = C * qvol, D_hat = D * qvol;
							u_best += discfact * W[m - cons - D_hat * b_best + C_hat * a_best][s + D_hat - C_hat] *
								P[C][D][da_best - (beta - alpha - ldelta)][db_best - (alpha - beta - udelta)];
							u += discfact * W[m - cons - D_hat * b + C_hat * a][s + D_hat - C_hat] *
								P[C][D][da - (beta - alpha - ldelta)][db - (alpha - beta - udelta)];
						}
					if (u_best < u)
					{
						a_best = a; b_best = b;
						v = u;
					}
				}

			b_best = m > 0 ? b_best : klundefprice;
			a_best = s > 0 ? a_best : khundefprice;
			last_ask = a_best;
			last_bid = b_best;

			//projection operator
			double w = epsparam * v + (1.0 - epsparam) * W[m][s];
			W[m][s] = w;
			for (int m_ind = 0; m_ind < W.size(); m_ind++)
				W[m_ind][s] = (m_ind < m) ? std::min(W[m_ind][s], w) : std::max(W[m_ind][s], w);
			for (int s_ind = 0; s_ind < W[0].size(); s_ind++)
				W[m][s_ind] = (s_ind < s) ? std::min(W[m][s_ind], w) : std::max(W[m][s_ind], w);


			trequest ord;
			if(b_best != klundefprice) ord.addbuylimit(b_best, qvol);
			if(a_best != khundefprice) ord.addselllimit(a_best, qvol);
			ord.setconsumption(cons);
			return ord;
		}


		tprice initprice;
		tvolume qvol;
		double discfact, epsparam;
		int Kparam, countKparam, bndmoney, bndstocks, ldelta, udelta;
		tvolume last_stocks;
		T2vec W;
		T4vec N, P;
		tprice last_bid, last_ask;

	};

} // namespace


#endif // ADPMARKETMAKER_HPP