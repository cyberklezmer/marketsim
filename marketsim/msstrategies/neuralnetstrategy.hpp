#include "marketsim.hpp"
#include "nets/actorcritic.hpp"
#include "nets/proba.hpp"


namespace marketsim {

    template<typename TNet, int reward_mult>
    class neuralnetstrategy : public teventdrivenstrategy {
    public:
        neuralnetstrategy() : teventdrivenstrategy(1),
            net(std::make_unique<TNet>()),
            state_history(),
			finitprice(100),
            gamma(0.99998),
            reward_scale(0) {
                optimizer = std::make_unique<torch::optim::SGD>(net->parameters(), /*lr=*/0.001);
            }

    private:
        virtual trequest event(const tmarketinfo& mi, tabstime t, trequestresult* lastresult) {
            auto money = mi.mywallet().money();
            auto stocks = mi.mywallet().stocks();

            if (reward_scale == 0) {
                reward_scale = reward_mult * (money + stocks);
            }

            auto ab = get_alpha_beta(mi);
            auto alpha = std::get<0>(ab);
            auto beta = std::get<1>(ab);
            
            std::vector<double> state_data = std::vector<double>{
                double(money) / reward_scale,
                double(stocks) / reward_scale,
                double(alpha) / reward_scale,
                double(beta) / reward_scale
            };
            auto next_state = torch::from_blob(state_data.data(), {1, 4});

            stocks = stocks * beta;  // current portfolio value estimate
            
            if (state_history.size() > 0) {
                auto prev_data = state_history.back();
                state_history.pop_back();

                auto state = std::get<0>(prev_data);
                auto action = std::get<1>(prev_data);
                auto consumption = std::get<2>(prev_data);

                //TODO consumption si spis jako tprice (nebo co) ulozit, budou se checkovat bounds atd
                // floor of predicted consumption
                tprice reward = money - prev_money + stocks - prev_stocks + int(consumption.item<double>());
                reward /= reward_scale;

                auto returns = reward + gamma * net->predict_values(next_state).detach();
                std::cout << "Returns " << returns << std::endl;
                std::cout << "Money: " << prev_money << " " << money << "Stocks: " << prev_stocks << " " << stocks;
                std::cout << std::endl << "Cons: " << int(consumption.item<double>()) << std::endl;

                train(state, action, consumption, returns);
            }

            auto pred_actions = net->predict_actions(next_state, true);
            torch::Tensor next_action = pred_actions.at(0);
            torch::Tensor next_cons = pred_actions.at(1);

            std::cout << "Actions ";
            std::cout << pred_actions << std::endl;
            std::cout << next_cons << std::endl << "AAAA\n";
            
            double apred = next_action[0][0].item<double>();
            double bpred = next_action[0][1].item<double>();
            double conspred = next_cons[0][0].item<double>();

            double lim = (money - mi.myorderprofile().B.value()) / 2;
            conspred = conspred >= lim ? lim : conspred; 

            state_history.push_back(std::make_tuple<>(next_state, next_action, next_cons));
            prev_money = money;
            prev_stocks = stocks;

            trequest ord;
			ord.addbuylimit(int(beta + bpred), 1);
			ord.addselllimit(int(alpha + apred), 1);  //TODO u vsech resit jak presne dat na int
			ord.setconsumption(int(conspred));
            return ord;
        }

        void train(const torch::Tensor& state, const torch::Tensor& actions, const torch::Tensor& consumption,
                   const torch::Tensor& returns)
        {
            optimizer->zero_grad();

            auto pred = net->forward(state);
            auto pred_actions = std::get<0>(pred);
            auto state_value = std::get<1>(pred);

            auto advantages = (returns - state_value);

            auto value_loss = advantages.pow(2).mean(); 
            //auto value_loss = at::huber_loss(state_value, returns);
            auto action_probs = net->action_log_prob(actions, consumption, pred_actions);
            auto action_loss = -(advantages.detach() * action_probs).mean(); //TODO minus správně?

            action_loss = at::clamp(action_loss, -5, 5);
            
            auto loss = value_loss + action_loss; //TODO entropy regularization

            std::cout << "Value " << value_loss << "Action: " << action_loss << std::endl;
            std::cout << "State_value: " << state_value << std::endl;

            loss.backward();
            optimizer->step();
        }

        std::pair<tprice, tprice> get_alpha_beta(const tmarketinfo& mi) {
			tprice alpha = mi.alpha();
			tprice beta = mi.beta();
            
			double p = (alpha != khundefprice && beta != klundefprice)
				? (alpha + beta) / 2 : finitprice;
            
			if (beta == klundefprice) beta = (last_bid == klundefprice) ? p - 1 : last_bid;
			if (alpha == khundefprice) alpha = (last_ask == khundefprice) ? p + 1 : last_ask;

            return std::make_pair<>(alpha, beta);
        }

        tprice prev_stocks, prev_money, prev_cons;
		tprice last_bid, last_ask;

        double finitprice;
        double gamma;
        tprice reward_scale;
        std::vector<std::tuple<torch::Tensor, torch::Tensor, torch::Tensor>> state_history;
        std::unique_ptr<TNet> net;
        std::unique_ptr<torch::optim::SGD> optimizer;
    };
}