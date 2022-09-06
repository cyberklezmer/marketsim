#ifndef GREEDYSTRATEGY_HPP_
#define GREEDYSTRATEGY_HPP_

#include "msneural/utils.hpp"
#include "marketsim.hpp"


namespace marketsim {


    template <int conslim, bool verbose = false, bool rand_actions = false, int cons = 500, int volume = 10,
              int keep_stocks = 10, int spread_size = 5, bool with_stocks = false>
    class greedystrategy : public teventdrivenstrategy {
    public:
        greedystrategy() :
            teventdrivenstrategy(1),
            last_bid(klundefprice),
            last_ask(khundefprice) {}

    private:
        virtual trequest event(const tmarketinfo& mi, tabstime t, trequestresult* lastresult) {
            auto ab = get_alpha_beta(mi, last_bid, last_ask);
            tprice a = std::get<0>(ab);
            tprice b = std::get<1>(ab);

            if (verbose) {
               std::cout << "Beta: " << b << ", Alpha: " << a << std::endl;
            }

            tprice bid = rand_actions ? get_random_int() : -spread_size;
            tprice ask = rand_actions ? get_random_int() : spread_size;

            tprice next_bid = b + bid;
            next_bid = (next_bid < 0) ? 1 : next_bid;

            tprice next_ask = a + ask;
            next_ask = (next_ask < 0) ? 1 : next_ask;

            auto ot = create_order(next_bid, next_ask, cons);
            if (verbose) {
                print_state(mi, next_bid, next_ask, cons, bid, ask);
            }

            last_bid = next_bid;
            last_ask = next_ask;
            
            return ot;
        }

        int get_random_int() {
            return random_int_from_tensor(-spread_size, spread_size + 1);
        }

        tprice last_bid, last_ask;
    };
}

#endif // GREEDYSTRATEGY_HPP_