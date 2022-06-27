#ifndef NEURALNETSTRATEGY_HPP_
#define NEURALNETSTRATEGY_HPP_

#include "nets/utils.hpp"
#include "marketsim.hpp"
#include <torch/torch.h>


namespace marketsim {

    template<typename TNet, int conslim, int keep_stocks, int spread_lim, int explore_cons, int volume = 10, bool verbose = true,
             bool modify_c = true, bool explore = true, bool limspread = true>
    class neuralnetstrategy : public teventdrivenstrategy {
    public:
        neuralnetstrategy() : teventdrivenstrategy(1),
            net(),
            history(),
            last_bid(klundefprice),
            last_ask(khundefprice) {}

    private:
        virtual trequest event(const tmarketinfo& mi, tabstime t, trequestresult* lastresult) {
            torch::Tensor next_state = this->construct_state(mi);
            net.train(history, next_state);

            auto pred_actions = net.predict_actions(next_state);
            torch::Tensor next_action = pred_actions.at(0);
            torch::Tensor next_cons = pred_actions.at(1);

            if (modify_c) {
                next_cons = limit_cons(mi, next_cons);
            }

            if (limspread) {
                next_action = limit_spread(next_action);
            }

            history.push_back(std::make_tuple<>(next_state, next_action, next_cons));
            return construct_order(mi, next_action, next_cons);
        }

        torch::Tensor limit_spread(torch::Tensor next_action) {
            double a1 = next_action[0][0].item<double>();
            double a2 = next_action[0][1].item<double>();
            
            if ((a1 >= spread_lim) || (a1 <= -spread_lim)) {
                next_action[0][0] = (a1 < 0) ? -spread_lim : spread_lim;
            }
            if ((a2 >= spread_lim) || (a2 <= -spread_lim)) {
                next_action[0][1] = (a2 < 0) ? -spread_lim : spread_lim;
            }

            return next_action;
        }

        torch::Tensor limit_cons(const tmarketinfo& mi, torch::Tensor cons) {
            double next_cons = cons[0][0].item<double>();
            next_cons = modify_consumption<conslim, verbose, explore_cons, explore>(mi, next_cons);
            return torch::tensor({next_cons}).reshape({1,1});
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

        virtual trequest construct_order(const tmarketinfo& mi, const torch::Tensor& actions, const torch::Tensor& cons) {
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

		tprice last_bid, last_ask;
        double finitprice;

        TNet net;
        std::vector<std::tuple<torch::Tensor, torch::Tensor, torch::Tensor>> history;
    };

}

#endif // NEURALNETSTRATEGY_HPP_