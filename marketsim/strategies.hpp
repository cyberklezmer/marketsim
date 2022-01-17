#ifndef STRATEGIES_HPP
#define STRATEGIES_HPP

#include "marketsim.hpp"

namespace marketsim
{

	///  A market making strategy based on approximate dynamic programming

	class tadpmarketmaker : public tsimplestrategy
	{
		using Tvec = std::vector<double>;
		using T2vec = std::vector<Tvec>;
		using T3vec = std::vector<T2vec>;
		using T4vec = std::vector<T3vec>;

	public:
		tadpmarketmaker(const std::string& name,
			tprice initprice,
			int bndmoney,
			int bndstocks,
			tvolume offeredvolume = 1,
			int ldelta = 25,
			int udelta = 45,
			double discfact = 0.9998,
			double epsparam = 0.2,
			int Kparam = 3000
		)
			: tsimplestrategy(name, 0),
			finitprice(initprice),
			fbndmoney(bndmoney),
			fbndstocks(bndstocks),
			fofferedvolume(offeredvolume),
			fldelta(ldelta),
			fudelta(udelta),
			fdiscfact(discfact),
			fepsparam(epsparam),
			fKparam(Kparam),
			W(T2vec(fbndmoney + 1, Tvec(fbndstocks + 1, 0.0))),
			N(T4vec(2, T3vec(2, T2vec(fldelta + fudelta + 1, Tvec(fldelta + fudelta + 1, 0.0))))),
			P(T4vec(2, T3vec(2, T2vec(fldelta + fudelta + 1, Tvec(fldelta + fudelta + 1, 0.0))))),
			last_bid(klundefprice),
			last_ask(khundefprice)
		{

		}
	private:
		virtual tsimpleorderprofile simpleevent(const tmarketinfo& mi, const ttradinghistory& th)
		{
			//initializations
			if (mi.t < std::numeric_limits<double>::epsilon())
			{
				//initialize W
				for (int i = 0; i <= fbndmoney; i++)
					for (int j = 0; j <= fbndstocks; j++)
						W[i][j] = (i == 0 && i == j) ? 0 : (1.0 - pow(fdiscfact, i + j * finitprice)) / (1.0 - fdiscfact);
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
				m = th.wallet().money();
				n = th.wallet().stocks();
			}

			double p = isnan(mi.history.p(mi.t)) ? finitprice : mi.history.p(mi.t);

			//update bid and ask (alpha, beta)
			tprice alpha = mi.orderbook.A.minprice();
			tprice beta = mi.orderbook.B.maxprice();

			if (beta == klundefprice) beta = (last_bid == klundefprice) ? p - 1 : last_bid;
			if (alpha == khundefprice) alpha = (last_ask == khundefprice) ? p + 1 : last_ask;

			//differences da, db; cut off according to bounds \fudelta, \fldelta
			tprice da = std::min(std::max(mi.history.a(mi.t) - alpha, beta - alpha - fldelta), beta - alpha + fudelta);
			tprice db = std::min(std::max(mi.history.b(mi.t) - beta, alpha - beta - fudelta), alpha - beta + fldelta);

			//update information (C,D) about the action of the market maker
			int d = 0, c = 0;
			if (th.wallet().stocks() < n) { c = 1; n--; }
			else if (th.wallet().stocks() > n) { d = 1; n++; }

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
			tprice a_best = alpha, b_best = beta, cash_best = 0;
			double v = 0.0;
			for (int cash = 0; cash <= 1; cash++)
				for (int b = alpha - fudelta; b > 0 && b <= alpha + fldelta && m - b - cash >= 0; b++)
					for (int a = beta - fldelta; a > b && a <= beta + fudelta; a++)
					{
						tprice da = std::min(std::max(a - alpha, beta - alpha - fldelta), beta - alpha + fudelta);
						tprice db = std::min(std::max(b - beta, alpha - beta - fudelta), alpha - beta + fldelta);
						tprice da_best = std::min(std::max(a_best - alpha, beta - alpha - fldelta), beta - alpha + fudelta);
						tprice db_best = std::min(std::max(b_best - beta, alpha - beta - fudelta), alpha - beta + fldelta);

						double u = 0.0, u_best = 0.0;
						for (int C = 0; C <= 1; C++)
							for (int D = 0; D <= 1; D++)
							{
								if(n + D - C < 0) continue;
								u_best += cash_best + fdiscfact * W[m - cash_best - D * b_best + C * a_best][n + D - C]
									* P[C][D][da_best - (beta - alpha - fldelta)][db_best - (alpha - beta - fudelta)];
								u += cash + fdiscfact * W[m - cash - D * b + C * a][n + D - C]
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

			tsimpleorderprofile ord;
			ord.a.volume = fofferedvolume;
			ord.b.volume = fofferedvolume;
			ord.a.price = a_best;
			ord.b.price = b_best;
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


	/// A liquidity taker strategy, issuing, at a predefined Poisson rate, market orders of a random type.
	class tliquiditytaker : public tsimplestrategy
	{
	public:
		/// constructor, \p interval is the average interval of arrival of the orders (i.e. the mean
		/// time between order is \c 1/intensity), \p volume is the ordered/offered volume of all the orders
		tliquiditytaker(const std::string& name,
			double interval, tvolume volume)
			: tsimplestrategy(name, interval, /* random=*/ true), fvolume(volume) {}
		virtual tsimpleorderprofile simpleevent(const tmarketinfo&,
			const ttradinghistory&)
		{
			bool buy = orpp::sys::uniform() < 0.5;
			return buy ? buymarket(fvolume) : sellmarket(fvolume);
		}
	private:
		tvolume fvolume;
	};

	/// Strategy rouhly mimcking the market making strategy described by Stoll (1978)
	class tstollmarketmaker : public tsimplestrategy
	{
	public:
		/// constructor
		/// \param name name
		/// \param initialprice midpoint price set in the beginning when there is no history
		/// \param desiredinventory the target value of the stock
		/// \param offeredvolume volume at bid and ask
		/// \param pestalpha exponential smoothing parameter for estimation of the "true" price
		/// \param dif2bias constant tranfering imbalance to deviation of midpoint and the fair price
		/// \param dif2percspread constant tranfering imbalance to spread
		tstollmarketmaker(const std::string& name,
			tprice initialprice,
			tvolume desiredinventory,
			tvolume offeredvolume = 1,
			double pestalpha = 0.1,

			double dif2bias = 0.005,
			double dif2percspread = 0.005
		)
			: tsimplestrategy(name, 0),
			finitialprice(initialprice),
			fofferedvolume(offeredvolume),
			fdesiredinventory(desiredinventory),
			fdif2percspread(dif2percspread),
			fdif2bias(dif2bias),
			fpestalpha(pestalpha)
		{}
	private:
		virtual tsimpleorderprofile simpleevent(const tmarketinfo& mi,
			const ttradinghistory& th)
		{
			double dt0 = log(0.001) / log(1 - fpestalpha);
			auto lat = tsimulation::def().flattency;
			double startt = floor(mi.t - dt0 * lat);
			double fairprice;
			if (startt <= 0 || isnan(mi.history.p(startt)))
				fairprice = finitialprice;
			else
				fairprice = mi.history.p(startt);
			for (double t = startt; t <= mi.t; t += lat)
			{
				if (t > 0 && !isnan(mi.history.p(t)))
					fairprice = (1 - fpestalpha) * fairprice
					+ fpestalpha * mi.history.p(t);
			}

			double D = fairprice * (th.wallet().stocks() - fdesiredinventory);
			double midpoint = fairprice - fdif2bias * D;
			double spread = std::max(1.0, fdif2percspread * fabs(D));
			tprice a = d2ptrim(midpoint + spread / 2);
			tprice b = d2ptrim(midpoint - spread / 2);

			if (a == b)
			{
				if (a == kminprice)
					a++;
				else if (a == kmaxprice)
					b--;
				else
					throw "zero spread out of edges";
			}

			if (tsimulation::islogging())
				orpp::sys::log() << '\t' << "fp: " << fairprice << ", mp: " << midpoint
				<< " spread: " << spread << " pbias:" << fdif2bias * D << std::endl;

			tsimpleorderprofile ret;
			ret.a.price = a;
			ret.b.price = b;
			ret.a.volume = ret.b.volume = fofferedvolume;
			return ret;
		}
		tprice finitialprice;
		tvolume fofferedvolume;
		tvolume fdesiredinventory;
		double fdif2percspread;
		double fdif2bias;
		double fpestalpha;
	};

	// Zero intelligence strategy as of Smith et. al (2003). TBD
	class zistrategy : public tstrategy
	{
	public:
		zistrategy(const std::string& name, double arrivalrate, double cancelationrate,
			unsigned minprice = 1, unsigned maxprice = 100) :
			tstrategy(name),
			farrivalrate(arrivalrate),
			fcancelationrate(cancelationrate),
			fmaxprice(maxprice),
			fminprice(minprice)
		{}


	private:
		double farrivalrate;
		double fcancelationrate;
		int fmaxprice;
		int fminprice;

		virtual trequest event(const tmarketinfo& mi,
			const ttradinghistory& /* aresult */)
		{
			unsigned numa = mi.myprofile.A.size();
			unsigned numb = mi.myprofile.B.size();
			double totalint = (numa + numb) * fcancelationrate + 2 * farrivalrate;
			double u = orpp::sys::uniform() * totalint;

			trequest::teraserequest er(false);
			tpreorderprofile rp;


			if (u < numa * fcancelationrate)
			{
				er.a = std::vector<bool>(numa, false);
				er.a[u / fcancelationrate] = true;
			}
			else
			{
				u -= numa * fcancelationrate;
				if (u < numb * fcancelationrate)
				{
					er.b = std::vector<bool>(numb, false);
					er.b[u / fcancelationrate] = true;
				}
				else
				{
					u -= numb * fcancelationrate;
					double v = orpp::sys::uniform() * (fmaxprice - fminprice + 1);
					unsigned num = orpp::sys::uniform() < 0.5 ? 1 : 2;
					if (u < farrivalrate)
						rp.A.add(tpreorder(fminprice + static_cast<tprice>(v), num));
					else
						rp.B.add(tpreorder(fminprice + static_cast<tprice>(v), num));
				}
			}
			std::exponential_distribution<> nu(totalint);
			//  a bit unprecise, should be corrected for present cancellations / insertions
			double nextupdate = nu(orpp::sys::engine());

			return trequest(rp, er, nextupdate);
		}


		virtual void atend(const ttradinghistory&)
		{

		}

		virtual void warning(trequest::ewarning, const std::string&)
		{
		}
	};



}

#endif // STRATEGIES_HPP
