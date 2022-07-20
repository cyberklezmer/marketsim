#ifndef NEURALNETSTRATEGY_HPP_
#define NEURALNETSTRATEGY_HPP_

#include "nets/utils.hpp"
#include "marketsim.hpp"
#include <torch/torch.h>


namespace marketsim {

    template<typename TNet, typename TOrder, int conslim, int spread_lim, int explore_cons, bool verbose = true,
             bool modify_c = true, bool explore = true, bool limspread = true, bool train_cons = true, int fixed_cons = 200,
             bool with_stocks = false>
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

            set_flags(pred_actions);

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

        void limit_cons(const tmarketinfo& mi, action_container<torch::Tensor>& next_action) {
            double next_cons = train_cons ? next_action.cons.item<double>() : fixed_cons;
            next_cons = modify_consumption<conslim, verbose, with_stocks, explore_cons, explore>(mi, next_cons);
            next_action.cons = torch::tensor({next_cons});
        }

        void set_flags(action_container<torch::Tensor>& next_action) {
            if (!next_action.is_flag_valid()) {
                return;
            }

            double bid = next_action.bid_flag.item<double>();
            double ask = next_action.ask_flag.item<double>();
            double threshold = 0.5;

            bid = (bid > threshold) ? 1.0 : 0.0;
            ask = (ask > threshold) ? 1.0 : 0.0;
            
            next_action.bid_flag = torch::tensor({bid}).reshape({1,1});
            next_action.ask_flag = torch::tensor({ask}).reshape({1,1});
        }

		tprice last_bid, last_ask;

        TNet net;
        TOrder order;
        std::vector<hist_entry> history;
    };

    template <int volume = 10, bool verbose = false, bool with_stocks = false>
    class neuralspeculatororder {
    public:
        neuralspeculatororder() : lim(0.5) {} 

        trequest construct_order(const tmarketinfo& mi, const action_container<torch::Tensor>& actions) {
            double bpred = actions.bid_flag.item<double>();
            double apred = actions.ask_flag.item<double>();
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

    template <int keep_stocks, int volume = 10, bool verbose = false, bool emergency = true>
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
            auto ot = create_order<volume, keep_stocks, verbose, emergency>(mi, bid, ask, int(conspred));
            if (verbose) {
                print_state(mi, ot, bpred, apred);
            }

            double bid_flag = (actions.is_flag_valid()) ? actions.bid_flag.item<double>() : 1.0;
            double ask_flag = (actions.is_flag_valid()) ? actions.ask_flag.item<double>() : 1.0;
            double threshold = 0.5;

            // set last bid/ask values
            if (ot.is_bid() && (bid_flag > threshold)) {
                last_bid = ot.get_bid();
            }
            if (ot.is_ask() && (ask_flag > threshold)) {
                last_ask = ot.get_ask();
            }

            return ot.to_request();
        }
    private:
        tprice last_bid, last_ask;
    };
}

#endif // NEURALNETSTRATEGY_HPP_