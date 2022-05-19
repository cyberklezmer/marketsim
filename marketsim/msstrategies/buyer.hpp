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

		}

	private:
		virtual trequest event(const tmarketinfo& info, tabstime t, trequestresult* lastresult)
		{
			if (!lastresult)
			{
				int bnd_ask_impact = 101; //change to sth dynamic
				cash = info.mywallet().money();
				W = T2vec(cash + 1, Tvec(bnd_ask_impact, 0.0));
			}

			price_last = price;
			price = (info.alpha() != khundefprice && info.beta() != klundefprice)
				? (info.alpha() + info.beta()) / 2 : 0;

			if(t < 10 && price && price_last)
			{
				cash = info.mywallet().money();
				ins.push_back(vol);
				outs.push_back(price - price_last);
				auto regress = getRegressCoef(ins, outs);

				vol = std::uniform_int_distribution<int>(0, std::floor(0.75 * cash / price))(gen);
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

	};

}