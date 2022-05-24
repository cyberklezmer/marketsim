#include "marketsim.hpp"
#include<random>
#include "leastsquares.hpp"

std::random_device rd;
std::mt19937 gen(rd());

namespace marketsim
{	
	class buyer : public teventdrivenstrategy 
	{
	public:
		buyer()
			: teventdrivenstrategy(1)
		{
			discfact = 0.9998;
			cash = 0;
			W = T2vec(0,Tvec(0,0.0));
			price = 0, price_last = 0;
			E = std::normal_distribution<>{ 0, E_stddev };
			F = std::normal_distribution<>{ 0, F_stddev };

		}

	private:
		virtual trequest event(const tmarketinfo& info, tabstime t, trequestresult* lastresult)
		{
			if (!lastresult)
			{
				int bnd_ask_impact = 101; //change to sth dynamic
				cash = info.mywallet().money();
				W = T2vec(cash + 1, Tvec(bnd_ask_impact, 0.0));
				return trequest();
			}

			price_last = price;
			price = (info.alpha() != khundefprice && info.beta() != klundefprice)
				? (info.alpha() + info.beta()) / 2 : std::numeric_limits<double>::quiet_NaN();

			if(!isnan(price - price_last))
			{
				cash = info.mywallet().money();
				ins.push_back(vol);
				outs.push_back(price - price_last);
				auto regress = getRegressCoef(ins, outs);

				for (tvolume v = 0; round(regress[0] + regress[1] * (v - 1)) + E_stddev < cash; v++);
				{
					double expv_opt = 0.0; double expv = 0.0;

					
				}

				trequest req;
				req.addbuymarket(vol);
				return req;
			}
			return trequest();
		}

		int vol, cash, cash_new, price_last, price;
		Tvec ins, outs;
		double discfact;
		T2vec W;
		double E_stddev, F_stddev;
		std::normal_distribution<> E, F;

	};

}