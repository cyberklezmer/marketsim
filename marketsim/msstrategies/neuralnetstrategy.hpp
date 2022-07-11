#ifndef NEURALNETSTRATEGY_HPP_
#define NEURALNETSTRATEGY_HPP_

#include "nets/utils.hpp"
#include "marketsim.hpp"
#include <torch/torch.h>


namespace marketsim {

    template<typename TNet, typename TOrder, int conslim, int spread_lim, int explore_cons, bool verbose = true,
             bool modify_c = true, bool explore = true, bool limspread = true>
    class neuralnetstrategy : public teventdrivenstrategy {
    public:
        neuralnetstrategy() : teventdrivenstrategy(1),
            net(),
            order(),
            history(),
            last_bid(klundefprice),
            last_ask(khundefprice) {}

    private:
        virtual trequest event(const tmarketinfo& mi, tabstime t, trequestresult* lastresult) {
            torch::Tensor next_state = this->construct_state(mi);
            net.train(history, next_state);

            auto pred_actions = net.predict_actions(history, next_state);

            if (modify_c) {
                limit_cons(mi, pred_actions);
            }

            if (limspread) {
                limit_spread(pred_actions);
            }

            history.push_back(hist_entry(next_state, pred_actions));
            return order.construct_order(mi, pred_actions);
        }
        
        torch::Tensor construct_state(const tmarketinfo& mi) {
            tprice m = mi.mywallet().money();
            tprice s = mi.mywallet().stocks();

            auto ab = get_alpha_beta(mi, last_bid, last_ask);
            tprice a = std::get<0>(ab);
            tprice b = std::get<1>(ab);
            
            std::vector<float> state_data = std::vector<float>{float(m), float(s), float(a), float(b)};
            return torch::tensor(state_data).reshape({1,4});
        }

        void limit_spread(action_container<torch::Tensor>& next_action) {
            double a1 = next_action.bid.item<double>();
            double a2 = next_action.ask.item<double>();
            
            if ((a1 >= spread_lim) || (a1 <= -spread_lim)) {
                a1 = (a1 < 0) ? -spread_lim : spread_lim;
                next_action.bid = torch::tensor({a1});
            }
            if ((a2 >= spread_lim) || (a2 <= -spread_lim)) {
                a2 = (a2 < 0) ? -spread_lim : spread_lim;
                next_action.ask = torch::tensor({a2});
            }
        }

        torch::Tensor limit_cons(const tmarketinfo& mi, action_container<torch::Tensor>& next_action) {
            double next_cons = next_action.cons.item<double>();
            next_cons = modify_consumption<conslim, verbose, explore_cons, explore>(mi, next_cons);
            next_action.cons = torch::tensor({next_cons});
        }

		tprice last_bid, last_ask;

        TNet net;
        TOrder order;
        std::vector<hist_entry> history;
    };

    template <int volume = 10, bool verbose = false>
    class neuralspeculatororder {
    public:
        neuralspeculatororder() : lim(0.5) {} 

        trequest construct_order(const tmarketinfo& mi, const action_container<torch::Tensor>& actions) {
            double bpred = actions.bid.item<double>();
            double apred = actions.ask.item<double>();
            double conspred = actions.cons.item<double>();

            if (verbose) {
                std::cout << "Bid: " << bpred << ", Ask: " << apred << ", Cons: " << conspred << std::endl;
                std::cout << "Wallet: " << mi.mywallet().money() << ", Stocks: " << mi.mywallet().stocks() << std::endl;
            }

            trequest o;
            if (bpred > lim) {
                o.addbuymarket(volume);
            }

            if (apred > lim) {
                o.addsellmarket(volume);
            }

            o.setconsumption(int(conspred));
            
            return o;
        }
    
    private:
        double lim;
    };

    template <int keep_stocks, int volume = 10, bool verbose = false>
    class neuralmmorder {
    public:
        neuralmmorder() : last_bid(klundefprice), last_ask(khundefprice) {}

        trequest construct_order(const tmarketinfo& mi, const action_container<torch::Tensor>& actions) {
            double bpred = actions.bid.item<double>();
            double apred = actions.ask.item<double>();
            double conspred = actions.cons.item<double>();

            auto ab = get_alpha_beta(mi, last_bid, last_ask);
            tprice a = std::get<0>(ab);
            tprice b = std::get<1>(ab);
            if (verbose) {
                std::cout << "Beta: " << b << ", Alpha: " << a << std::endl;
            }

            tprice bid = int(b + bpred);
            tprice ask = int(a + apred);
            
            bid = (bid < 0) ? 0 : bid;
            ask = (ask <= 0) ? 1 : ask;

            if (bid >= ask) {
                ask = bid + 1;
            }

            // create order template
            auto ot = create_order<volume, keep_stocks, verbose>(mi, bid, ask, int(conspred));
            if (verbose) {
                print_state(mi, ot, bpred, apred);
            }

            auto is_valid = [&](double what){ return !actions.is_flag_valid() || (what > 0.5); };
            double bid_flag = actions.bid_flag.item<double>();
            double ask_flag = actions.ask_flag.item<double>();

            // set last bid/ask values
            if (ot.is_bid() && is_valid(bid_flag)) {
                last_bid = ot.get_bid();
            }
            if (ot.is_ask() && is_valid(ask_flag)) {
                last_ask = ot.get_ask();
            }

            return ot.to_request();
        }
    private:
        tprice last_bid, last_ask;
    };
}

#endif // NEURALNETSTRATEGY_HPP_