#ifndef GREEDYSTRATEGY_HPP_
#define GREEDYSTRATEGY_HPP_

#include "nets/utils.hpp"
#include "marketsim.hpp"


namespace marketsim {


    template <int conslim, bool verbose = false, bool rand_actions = false, int cons = 500, int volume = 10,
              int keep_stocks = 10, int spread_size = 5>
    class greedystrategy : public teventdrivenstrategy {
    public:
        greedystrategy() :
            teventdrivenstrategy(1), 
			finitprice(100),
            last_bid(klundefprice),
            last_ask(khundefprice) {}

    private:
        virtual trequest event(const tmarketinfo& mi, tabstime t, trequestresult* lastresult) {
            tprice m = mi.mywallet().money();// - mi.myorderprofile().B.value();
            tprice s = mi.mywallet().stocks();

            auto ab = get_alpha_beta(mi, last_bid, last_ask);
            tprice a = std::get<0>(ab);
            tprice b = std::get<1>(ab);

            if (verbose) {
               std::cout << "Beta: " << b << ", Alpha: " << a << std::endl;
            }

            tprice bid = rand_actions ? get_random_int() : spread_size;
            tprice ask = rand_actions ? get_random_int() : -spread_size;

            tprice next_bid = b + bid;
            next_bid = (next_bid < 0) ? 1 : next_bid;

            tprice next_ask = a + ask;
            next_ask = (next_ask < 0) ? 1 : next_ask;

            int next_cons = modify_consumption<conslim, verbose>(mi, cons);
            auto ot = create_order<volume, keep_stocks, verbose>(mi, next_bid, next_ask, next_cons);
            if (verbose) {
                print_state(mi, ot, bid, ask);
            }

            // set last bid/ask values
            if (ot.is_bid()) {
                last_bid = ot.get_bid();
            }
            if (ot.is_ask()) {
                last_ask = ot.get_ask();
            }
            return ot.to_request();
        }

        int get_random_int() {
            torch::Tensor rand_tens = torch::randint(-spread_size, spread_size + 1, {1});
            return rand_tens.item<int>();
        }

        tprice last_bid, last_ask, finitprice;
    };
}

#endif // GREEDYSTRATEGY_HPP_