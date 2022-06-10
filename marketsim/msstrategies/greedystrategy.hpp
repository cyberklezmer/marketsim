#ifndef GREEDYSTRATEGY_HPP_
#define GREEDYSTRATEGY_HPP_

#include "neuralnetstrategy.hpp"  //TODO dat do nejakejch utils to alpha beta
#include "marketsim.hpp"


namespace marketsim {


    template <int cons = 10000, int volume = 10, int keep_stocks = 10, int bid = -10, int ask = 10>
    class greedystrategy : public teventdrivenstrategy {
    public:
        greedystrategy() :
            teventdrivenstrategy(1), 
			finitprice(100),
            last_bid(klundefprice),
            last_ask(khundefprice) {}

    private:
        virtual trequest event(const tmarketinfo& mi, tabstime t, trequestresult* lastresult) {
            tprice m = mi.mywallet().money() - mi.myorderprofile().B.value();
            tprice s = mi.mywallet().stocks();

            auto ab = get_alpha_beta(mi, finitprice, last_bid, last_ask);
            tprice a = std::get<0>(ab);
            tprice b = std::get<1>(ab);
            std::cout << "Beta: " << b << ", Alpha: " << a << std::endl;

            trequest ord;

            last_bid = b + bid;
            last_bid = (last_bid < 0) ? 1 : last_bid;
            if (m > last_bid) {
                ord.addbuylimit(last_bid, volume);
                std::cout << "Bid: " << last_bid;
            }

            last_ask = a + ask;
            last_ask = (last_ask < 0) ? 1 : last_ask;
            if (s > keep_stocks) {
			    ord.addselllimit(last_ask, volume);
                std::cout << ", Ask: " << last_ask;
            }

            if (m > cons) {
                ord.setconsumption(cons);
                std::cout << ", Cons: " << cons;
            }

            std::cout << std::endl << "Wallet: " << m << ", Stocks: " << s << std::endl;

            return ord;
        }

        tprice last_bid, last_ask, finitprice;
    };
}

#endif // GREEDYSTRATEGY_HPP_