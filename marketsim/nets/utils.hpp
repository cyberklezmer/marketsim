#ifndef NET_UTILS_HPP_
#define NET_UTILS_HPP_

#include "marketsim.hpp"
#include <torch/torch.h>
using hist_entry = std::tuple<torch::Tensor, torch::Tensor, torch::Tensor>;

namespace marketsim {
    std::vector<torch::Tensor> get_past_states(const std::vector<hist_entry>& history, int start, int steps) {
        int idx = history.size() - start - steps;
        std::vector<torch::Tensor> states;

        for (int i = idx; i < idx + steps; ++i) {
            auto entry = history.at(i);
            states.push_back(std::get<0>(entry));
        }

        return states;
    }

    torch::Tensor stack_state_history(std::vector<torch::Tensor>& history, int axis) {
        return torch::cat(history, axis);
    }

    torch::Tensor stack_state_history(std::vector<torch::Tensor>& history, torch::Tensor next_state, int axis) {
        history.push_back(next_state);
        return stack_state_history(history, axis);
    }

    template <tprice finitprice = 100>
    std::pair<tprice, tprice> get_alpha_beta(const tmarketinfo& mi, tprice last_bid, tprice last_ask)
    {
			tprice alpha = mi.alpha();
			tprice beta = mi.beta();
            
			double p = (alpha != khundefprice && beta != klundefprice)
				? (alpha + beta) / 2 : finitprice;
            
			if (beta == klundefprice) beta = (last_bid == klundefprice) ? p - 1 : last_bid;
			if (alpha == khundefprice) alpha = (last_ask == khundefprice) ? p + 1 : last_ask;

            return std::make_pair<>(alpha, beta);
    }

    template <int conslim, bool verbose = false, int explore_cons = 0, bool explore = false>
    double modify_consumption(const tmarketinfo& mi, double conspred) {
            // try to do some nonzero consumption
            if (explore) {
                torch::Tensor randn = torch::rand({1});

                if ((randn < 0.1).item<bool>()) {
                    conspred = explore_cons;
                    if (verbose) {
                        std::cout << "modify" << std::endl;
                    }
                }
            }

            // don't consume if money low
            double lim = mi.mywallet().money();// - mi.myorderprofile().B.value();
            if ((lim - conspred) < conslim) {
                conspred = 0.0;
            }
            if (conspred < 0) {
                conspred = 0.0;
            }

            return conspred;
    }

    class ordertemplate {
    public:
        ordertemplate(bool erase) :
            cons(0), erase(erase), _is_bid(false), _is_ask(false), _is_cons(false) {}

        void set_bid(tprice bid, int volume) {
            this->bid = bid;
            this->bv = volume;
            _is_bid = true;
        }

        void set_ask(tprice ask, int volume) {
            this->ask = ask;
            this->av = volume;
            _is_ask = true;
        }

        void set_cons(tprice cons) {
            this->cons = cons;
            _is_cons = true;
        }

        trequest to_request() {
            tpreorderprofile pp;
            trequest ord;

            if (_is_bid) {
                ord.addbuylimit(bid, bv);
                pp.B.add(tpreorder(bid, bv));
            }

            if (_is_ask) {
                ord.addselllimit(ask, av);
                pp.A.add(tpreorder(ask, av));
            }

            ord.setconsumption(cons);

            if (erase) {
                return {pp, trequest::teraserequest(true), cons};
            }
            return ord;
        }
    
        tprice get_bid() { return bid; }
        tprice get_ask() { return ask; }
        tprice get_cons() { return cons; }

        bool is_bid() { return _is_bid; }
        bool is_ask() { return _is_ask; }
        bool is_cons() { return _is_cons; }

    private:
        int bv, av;
        bool _is_bid, _is_ask, _is_cons, erase;
        tprice bid, ask, cons;
    };

    void print_state(const tmarketinfo& mi, ordertemplate& ot, int bpred = 0, int apred = 0) {
        std::cout << "Bid: " << (ot.is_bid() ? std::to_string(ot.get_bid()) : std::string(" ")) << "(" << bpred << ")";
        std::cout << ", Ask: " << (ot.is_ask() ? std::to_string(ot.get_ask()) : std::string(" ")) << "(" << apred << ")";
        std::cout << ", Cons: " << (ot.is_cons() ? ot.get_cons() : 0) << std::endl;
        std::cout << "Wallet: " << mi.mywallet().money() << ", Stocks: " << mi.mywallet().stocks() << std::endl;
    }

    template <int volume, int keep_stocks, bool verbose = false, bool erase = true>
    ordertemplate create_order(const tmarketinfo& mi, tprice bid, tprice ask, tprice cons) {
			ordertemplate ot(erase);
            
            tprice m = mi.mywallet().money();
            tprice s = mi.mywallet().stocks();

            bool vol_enough = m > (bid * volume);
            bool b_enough = m > bid;
            bool stocks_enough = s > keep_stocks;

            if (vol_enough || b_enough) {
                int bid_vol = vol_enough ? volume : 1;
                ot.set_bid(bid, bid_vol);
            }

            if (stocks_enough) {
                ot.set_ask(ask, volume);
            }

            // get out of low resources
            if (!stocks_enough && !b_enough) {
                if (verbose) {
                    std::cout << "emergency" << std::endl;
                }

                bid = std::min(int(m / 2), bid);
                ot.set_bid(bid, 1);
                ot.set_ask(ask, 1);
            }

            ot.set_cons(cons);

            return ot;
    }
}

#endif  //NET_UTILS_HPP_