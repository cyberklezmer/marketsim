#ifndef NEURALNETSTRATEGY_HPP_
#define NEURALNETSTRATEGY_HPP_

#include "marketsim.hpp"
#include <torch/torch.h>


namespace marketsim {

    std::pair<tprice, tprice> get_alpha_beta(const tmarketinfo& mi, tprice finitprice,
                                             tprice last_bid, tprice last_ask);

    template<typename TNet, int conslim, int keep_stocks, int spread_lim, bool modify_c = true, int volume = 10>
    class neuralnetstrategy : public teventdrivenstrategy {
    public:
        neuralnetstrategy() : teventdrivenstrategy(1),
            net(),
            history(),
			finitprice(100),
            last_bid(klundefprice),
            last_ask(khundefprice),
            round(0) {}

    private:
        virtual trequest event(const tmarketinfo& mi, tabstime t, trequestresult* lastresult) {
            torch::Tensor next_state = this->construct_state(mi);
            net.train(history, next_state);

            auto pred_actions = net.predict_actions(next_state);
            torch::Tensor next_action = pred_actions.at(0);
            torch::Tensor next_cons = pred_actions.at(1);

            if (modify_c) {
                next_cons = this->modify_consumption(mi, next_cons);
            }

            double a1 = next_action[0][0].item<double>();
            double a2 = next_action[0][1].item<double>();
            if ((a1 >= spread_lim) || (a1 <= -spread_lim)) {
                next_action[0][0] = (a1 < 0) ? -spread_lim : spread_lim;
            }
            if ((a2 >= spread_lim) || (a2 <= -spread_lim)) {
                next_action[0][1] = (a2 < 0) ? -spread_lim : spread_lim;
            }

            history.push_back(std::make_tuple<>(next_state, next_action, next_cons));

            round += 1;
            return construct_order(mi, next_action, next_cons);
        }

        virtual torch::Tensor construct_state(const tmarketinfo& mi) {
            tprice m = mi.mywallet().money();
            tprice s = mi.mywallet().stocks();

            auto ab = get_alpha_beta(mi, finitprice, last_bid, last_ask);
            tprice a = std::get<0>(ab);
            tprice b = std::get<1>(ab);
            
            std::vector<float> state_data = std::vector<float>{float(m), float(s), float(a), float(b)};
            return torch::tensor(state_data).reshape({1,4});
        }

        virtual torch::Tensor modify_consumption(const tmarketinfo& mi, const torch::Tensor& cons) {
            double conspred = cons[0][0].item<double>();

            torch::Tensor randn = torch::rand({1});
            if ((randn < 0.1).item<bool>()) {
                conspred = 125;
                std::cout << "modify" << std::endl;
            }

            double lim = mi.mywallet().money();// - mi.myorderprofile().B.value();
            if ((lim - conspred) < conslim) {
                conspred = 0.0;
            }

            if (conspred < 0) {
                conspred = 0.0;
            }

            return torch::tensor({conspred}).reshape({1,1});
        }

        virtual trequest construct_order(const tmarketinfo& mi, const torch::Tensor& actions, const torch::Tensor& cons) {
            double bpred = actions[0][0].item<double>();
            double apred = actions[0][1].item<double>();
            double conspred = cons[0][0].item<double>();

            auto ab = get_alpha_beta(mi, finitprice, last_bid, last_ask);
            tprice a = std::get<0>(ab);
            tprice b = std::get<1>(ab);
            std::cout << "Beta: " << b << ", Alpha: " << a << std::endl;

            tprice bid = int(b + bpred);
            tprice ask = int(a + apred);
            
            bid = (bid < 0) ? 0 : bid;
            ask = (ask <= 0) ? 1 : ask;

            if (bid >= ask) {
                ask = bid + 1;
            }

            tpreorderprofile pp;

            trequest ord;
            bool is_bid = false;
            bool is_ask = false;
			tprice m = mi.mywallet().money();
            tprice s = mi.mywallet().stocks();

            if (m > (bid * volume)) {
                ord.addbuylimit(bid, volume);
                pp.B.add(tpreorder(bid, volume));
                last_bid = bid;
                is_bid = true;
            }

            if (s > keep_stocks) {
			    ord.addselllimit(ask, volume);  //TODO u vsech resit jak presne dat na int
                pp.A.add(tpreorder(ask, volume));
                last_ask = ask;
                is_ask = true;
            }

            // get out of low resources
            if (s <= keep_stocks && m <= (b * volume)) {
                std::cout << "emergency" << std::endl;

                last_bid = int(m / 2);
                last_ask = a - 5;
                ord.addbuylimit(last_bid, 1);
                pp.B.add(tpreorder(last_bid, 1));
                ord.addselllimit(last_ask, 1);
                pp.A.add(tpreorder(last_ask, 1));
                is_bid = true;
                is_ask = true;
            }

            ord.setconsumption(int(conspred));

            std::cout << "Bid: " << (is_bid ? std::to_string(last_bid) : std::string(" ")) << "(" << bpred << ")";
            std::cout << ", Ask: " << (is_ask ? std::to_string(last_ask) : std::string(" ")) << "(" << apred << ")";
            std::cout << ", Cons: " << int(conspred) << std::endl;
            std::cout << "Wallet: " << m << ", Stocks: " << s << std::endl;


            return {pp, trequest::teraserequest(true), int(conspred)};
            //return ord;
        }

        int round;
		tprice last_bid, last_ask;
        double finitprice;

        TNet net;
        std::vector<std::tuple<torch::Tensor, torch::Tensor, torch::Tensor>> history;
    };

    std::pair<tprice, tprice> get_alpha_beta(const tmarketinfo& mi, tprice finitprice,
                                             tprice last_bid, tprice last_ask)
    {
			tprice alpha = mi.alpha();
			tprice beta = mi.beta();
            
			double p = (alpha != khundefprice && beta != klundefprice)
				? (alpha + beta) / 2 : finitprice;
            
			if (beta == klundefprice) beta = (last_bid == klundefprice) ? p - 1 : last_bid;
			if (alpha == khundefprice) alpha = (last_ask == khundefprice) ? p + 1 : last_ask;

            return std::make_pair<>(alpha, beta);
    }
}

#endif // NEURALNETSTRATEGY_HPP_