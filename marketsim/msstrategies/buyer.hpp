#include "marketsim.hpp"
#include <boost/math/distributions/normal.hpp>
#include<boost/math/distributions/poisson.hpp>
	using boost::math::normal;
	using boost::math::poisson;

namespace marketsim
{	
	class buyer : public teventdrivenstrategy 
	{
		using Tvec = std::vector<double>;
		using T2vec = std::vector<std::vector<double>>;

	public:
		buyer() : teventdrivenstrategy(1)
		{
			discfact = 0.999;
			a_last = khundefprice, a = khundefprice;
			vol_last = 0;
			E_mean = 0, E_stddev = 1;
			F_mean = 0, F_stddev = 1;
			p_m = 0.01, k_m = 100, lambda_m = 3;
		}

	private:
		virtual trequest event(const tmarketinfo& info, tabstime t, trequestresult* lastresult)
		{	
				
			if (!lastresult)
			{

				m = info.mywallet().money(), s = info.mywallet().stocks(), a = info.a(), vol_last = 0;
				bnd_ask = 1000, bnd_m = 100000; //change to sth dynamic
				W = T2vec(bnd_m + 1, Tvec(bnd_ask + 1, 0.0));
				for (int i = 1; i <= bnd_m; i++)
					for (int j = 1; j <= bnd_ask; j++)
						W[i][j]  = 1.0 * i / j;
			}

			m_last = m, s_last = s, a_last = a;
			m = info.mywallet().money(), a = info.a(), s = info.mywallet().stocks();
			auto diff_m = abs(m - m_last), diff_s = abs(s - s_last);

			if(a != khundefprice && a_last != khundefprice)
			{
				if (vol_last > 0) ins.push_back(vol_last);
				if (diff_s > 0) outs.push_back(1.0 * (diff_m - a_last) / diff_s);
				auto coefs = ins.size() > 5 && ins.size() == outs.size() ? 
					getRegressCoef(ins, outs) : Tvec{0,2};

				tvolume vol_best = 0;
				double w = 0;

				for (tvolume v = 0; round(coefs[0] + coefs[1] * (v - 1) + E_stddev) < m; v++)
				{
					double expv_opt = 0.0, expv = 0.0;

					for (int e = round(E_mean - 4 * E_stddev); e <= round(E_mean + 4 * E_stddev); e++)
					{ 
						double pr_e = 
							boost::math::cdf(normal(E_mean, E_stddev), e + 0.5) -
							boost::math::cdf(normal(E_mean, E_stddev), e - 0.5);

						for (int f = round(F_mean - 4 * F_stddev); f <= round(F_mean + 4 * F_stddev); f++)
						{
							double pr_f = 
								boost::math::cdf(normal(F_mean, F_stddev), f + 0.5) -
								boost::math::cdf(normal(F_mean, F_stddev), f - 0.5);

							for (int c = std::max<int>(0, lambda_m - 4*sqrt(lambda_m)); c < lambda_m + 4 * sqrt(lambda_m); c++)
							{ 
								double pr_c = boost::math::pdf(poisson(lambda_m), c);

								for (int y = 0; y <= 1; y++)
								{
									double prob = pr_e * pr_f * pr_c * (y ? (1 - p_m) : p_m);

									unsigned ind_m = std::min(
										std::max<int>(0, m + y * k_m * c - v * (a + round(coefs[0] + coefs[1] * (v - 1) + e))),
										bnd_m);

									unsigned ind_ask = std::min(std::max(0, a + f), bnd_ask);

									expv += v + discfact * W[ind_m][ind_ask] * prob;

									ind_m = std::min(
										std::max<int>(0, m + y * k_m * c - vol_best * (a + round(coefs[0] +coefs[1] * (vol_best - 1) + e))),
										bnd_m);

									expv_opt += vol_best + discfact * W[ind_m][ind_ask] * prob;
								}	
							}
						}
					}
					if (expv > expv_opt)
					{
						vol_best = v;
						w = expv;
					}
				}

				W[m][a] = w;
				trequest req;
				req.addbuymarket(vol_best);
				vol_last = vol_best;
				return req;
			}

			return trequest();
		}

		Tvec getRegressCoef(Tvec ins, Tvec outs, bool intercept = false)
		{
			double xavg = std::accumulate(ins.begin(), ins.end(), 0.0) / ins.size();
			double yavg = std::accumulate(outs.begin(), outs.end(), 0.0) / outs.size();

			int n = ins.size();
			double xy = 0.0, xx = 0.0,
				c = intercept ? n * xavg * yavg : 0, d = intercept ? n * xavg * xavg : 0;
			for (int i = 0; i < n; i++)
			{
				xy += ins[i] * outs[i];
				xx += ins[i] * ins[i];
			}
			double beta = (xy - c) / (xx - d);

			return Tvec{ intercept * (yavg - beta) * xavg, beta };
		}

		tvolume vol_last;
		tprice m, m_last, a, a_last;
		tvolume s, s_last;
		Tvec ins, outs;
		double discfact;
		T2vec W;
		double E_stddev, F_stddev, E_mean, F_mean;
		double p_m, k_m, lambda_m;
		int bnd_m, bnd_ask;

        private:

               virtual bool acceptsdemand() const  { return true; }
        };

}
