#include "marketsim.hpp"
#include <torch/torch.h>


namespace marketsim {

    std::pair<tprice, tprice> get_alpha_beta(const tmarketinfo& mi, tprice finitprice,
                                             tprice last_bid, tprice last_ask);

    template<typename TNet>
    class neuralnetstrategy : public teventdrivenstrategy {
    public:
        neuralnetstrategy() : teventdrivenstrategy(1),
            net(),
            history(),
			finitprice(100),
            last_bid(klundefprice),
            last_ask(khundefprice) {}

    private:
        virtual trequest event(const tmarketinfo& mi, tabstime t, trequestresult* lastresult) {
            torch::Tensor next_state = this->construct_state(mi);
            net.train(history, next_state);

            auto pred_actions = net.predict_actions(next_state);
            torch::Tensor next_action = pred_actions.at(0);
            torch::Tensor next_cons = pred_actions.at(1);
            next_cons = this->modify_consumption(mi, next_cons);

            std::cout << "\nActions: " << next_action << "\nConsume: " << next_cons << std::endl;

            history.push_back(std::make_tuple<>(next_state, next_action, next_cons));

            return construct_order(mi, next_action, next_cons);
        }

        virtual torch::Tensor construct_state(const tmarketinfo& mi) {
            auto m = mi.mywallet().money();
            auto s = mi.mywallet().stocks();

            auto ab = get_alpha_beta(mi, finitprice, last_bid, last_ask);
            auto a = std::get<0>(ab);
            auto b = std::get<1>(ab);
            
            std::vector<float> state_data = std::vector<float>{float(m), float(s), float(a), float(b)};

            std::cout << state_data << std::endl;

            auto options = torch::TensorOptions().dtype(torch::kFloat32);
            return torch::from_blob((float*)state_data.data(), {1, 4}, options);
        }

        virtual torch::Tensor modify_consumption(const tmarketinfo& mi, const torch::Tensor& cons) {
            double conspred = cons[0][0].item<double>();

            double lim = (mi.mywallet().money() - mi.myorderprofile().B.value()) / 2;
            conspred = conspred >= lim ? lim : conspred;
            conspred = conspred < 0 ? 1.0 : conspred;

            return torch::tensor({conspred}).reshape({1,1});
        }

        virtual trequest construct_order(const tmarketinfo& mi, const torch::Tensor& actions, const torch::Tensor& cons) {
            double bpred = actions[0][1].item<double>();
            double apred = actions[0][1].item<double>();
            double conspred = cons[0][0].item<double>();

            auto ab = get_alpha_beta(mi, finitprice, last_bid, last_ask);
            auto a = std::get<0>(ab);
            auto b = std::get<1>(ab);

            last_bid = int(b + bpred);
            last_ask = int(a + apred);

            trequest ord;
			ord.addbuylimit(last_bid, 1);
			ord.addselllimit(last_ask, 1);  //TODO u vsech resit jak presne dat na int
			ord.setconsumption(int(conspred));

            return ord;
        }

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