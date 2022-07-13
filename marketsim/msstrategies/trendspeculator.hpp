#include "marketsim.hpp"

namespace marketsim
{

	///  A speculative strategy using price trend to optimize

	class trendspeculator : public teventdrivenstrategy
	{

		using Tvec = std::vector<double>;
		using T2vec = std::vector<Tvec>;
		using T3vec = std::vector<T2vec>;

	public:
		trendspeculator() : teventdrivenstrategy(1)
		{
			initprice = 100;
			discfact = 0.999;
			qvol = 1;
			trend_size = 3;
			max_jump = 100;
		}

		virtual trequest event(const tmarketinfo& info, tabstime t, trequestresult* lrr)
		{
			bool firsttime = lrr == 0;
			if (firsttime)
			{
				p_last = p_curr = klundefprice;
				hist = std::deque<int>();
				bnd_money = info.mywallet().money() * 10;
				bnd_stocks = info.mywallet().stocks() + 100;

				int no_trends = int(std::pow(3, trend_size));
				shift_freq = shift_distr = T2vec(no_trends, Tvec(2 * max_jump + 1, 0.0));
				V = T3vec(bnd_money + 1, T2vec(bnd_stocks + 1, Tvec(no_trends, 0.0)));

				//init V
				for (int i = 0; i <= bnd_money; i++)
				{ 
					for (int j = 0; j <= bnd_stocks; j++)
					{ 
						for (int h = 0; h < no_trends; h++)
						{ 
							V[i][j][h] = (i == 0 && i == j) ? 0 :
								(1.0 - std::pow(discfact, i + j * initprice)) / (1.0 - discfact);
						}
					}
				}

				//init distribution
				for (int h = 0; h < no_trends; h++)
				{
					for (int p = 0; p < 2 * max_jump + 1; p++)
					{
						int incr = 0;
						if (p > 0.8 * max_jump  && p < 1.2*max_jump)
							incr = 20;
						else if (p > 0.6*max_jump && p < 1.4*max_jump)
							incr = 10;
						else
							incr = 2;

						shift_freq[h][p] = incr;
					}

				}

			}

			p_last = p_curr;

			tprice alpha = info.alpha(), beta = info.beta();
			p_curr = (alpha != khundefprice && beta != klundefprice)
				? (alpha + beta) / 2 : klundefprice;

			trequest req;

			if (p_curr != klundefprice && p_last != klundefprice)
			{
				int lastmovement = 1;
				auto jump = std::max(-max_jump, std::min(p_curr - p_last, max_jump));
				if (jump) lastmovement = jump > 0 ? 2 : 0;

				if (hist.size() < trend_size)
					hist.push_front(lastmovement);
				else if (hist.size() == trend_size)
				{
					shift_freq[convert(hist)][max_jump + jump]++;
					hist.pop_back();
					hist.push_front(lastmovement);

					shift_distr = normalize(shift_freq);
					int h = convert(hist);

					double v = 0.0;
					auto m = info.mywallet().money(), s = info.mywallet().stocks();
					int d_best = 0, c_best = 0;

					for (int d = -1; d <= 1 && s - qvol >= 0; d++)
					{
						for (int c = 0; m - c - qvol * alpha >= 0; c++)
						{
							double expv = 0.0, expv_best = 0.0;
							for (int q = -max_jump; q <= max_jump; q++)
							{
								expv += c +
									discfact * V[m - d * (p_curr + q)][s + d][h] * shift_distr[h][q + max_jump];
								expv_best += c_best +
									discfact * V[m - d * (p_curr + q)][s + d_best][h] * shift_distr[h][q + max_jump];
							}

							if (expv > expv_best)
							{
								c_best = c;
								d_best = d;
								v = expv;
							}
						}
					}

					V[m][s][h] = v;
					req.setconsumption(c_best);
					if (d_best == 1) req.addbuymarket(qvol);
					else if (d_best == -1) req.addsellmarket(qvol);
				}
			}

			return req;
		}

		int convert(std::deque<int> d)
		{
			int index = 0;
			for (int i = 0; i < d.size(); i++)
				index += d[d.size() - 1 - i] * std::pow(3, i);

			return index;
		}

		T2vec normalize(T2vec A)
		{
			auto out = A;
			for (int i = 0; i < A.size(); i++)
			{
				double sum = 0.0;
				for (int j = 0; j < A[0].size(); j++)
					sum += A[i][j];

				if (sum > 0)
				{
					for (int j = 0; j < A[0].size(); j++)
						out[i][j] = A[i][j] / sum;
				}
			}
			return out;
		}

		tprice initprice;
		tvolume qvol;
		tprice p_last, p_curr;
		int bnd_money, bnd_stocks;
		int trend_size;
		tprice max_jump;
		double discfact;
		std::deque<int> hist;
		T2vec shift_freq, shift_distr;
		T3vec V;
	};

		}