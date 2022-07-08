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
            torch::Tensor next_action = pred_actions.at(0);
            torch::Tensor next_cons = pred_actions.at(1);

            if (modify_c) {
                limit_cons(mi, pred_actions);
            }

            if (limspread) {
                limit_spread(pred_actions);
            }

            //TODO create reward
            history.push_back(std::make_tuple<>(next_state, next_action, next_cons));
            return order.construct_order(mi, next_action, next_cons);
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

        trequest construct_order(const tmarketinfo& mi, const torch::Tensor& actions, const torch::Tensor& cons) {
            double bpred = actions[0][0].item<double>();
            double apred = actions[0][1].item<double>();
            double conspred = cons[0][0].item<double>();

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

        trequest construct_order(const tmarketinfo& mi, const torch::Tensor& actions, const torch::Tensor& cons) {
            double bpred = actions[0][0].item<double>();
            double apred = actions[0][1].item<double>();
            double conspred = cons[0][0].item<double>();

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

            // set last bid/ask values
            if (ot.is_bid()) {
                last_bid = ot.get_bid();
            }
            if (ot.is_ask()) {
                last_ask = ot.get_ask();
            }

            return ot.to_request();
        }
    private:
        tprice last_bid, last_ask;
    };
}

#endif // NEURALNETSTRATEGY_HPP_