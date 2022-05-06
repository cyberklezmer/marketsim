#include "marketsim.hpp"
#include "nets/actorcritic.hpp"
#include "nets/proba.hpp"


namespace marketsim {

    template<typename TNet>
    class neuralnetstrategy : public teventdrivenstrategy {
    public:
        neuralnetstrategy() : teventdrivenstrategy(1),
            net(std::make_unique<TNet>()),
            optimizer(std::make_unique<torch::optim::SGD>(net->parameters(), /*lr=*/0.01)),
            history(),
			finitprice(100),
            gamma(0.99998) {}

    private:
        virtual trequest event(const tmarketinfo& mi, tabstime t, trequestresult* lastresult) {
            auto next_state = nullptr;  //TODO

            auto money = mi.mywallet().money();
            auto stocks = mi.mywallet().stocks();

            auto ab = get_alpha_beta(mi);
            auto alpha = std::get<0>(ab);
            auto beta = std::get<1>(ab);

            stocks = stocks * beta;  // current portfolio value estimate
            
            if (history.size() > 0) {
                auto prev_data = history.back();
                history.pop_back();

                auto state = std::get<0>(prev_data);
                auto action = std::get<1>(prev_data);
                auto consumption = std::get<2>(prev_data);

                //TODO consumption si spis jako tprice (nebo co) ulozit, budou se checkovat bounds atd
                // floor of predicted consumption
                tprice reward = prev_money - money + prev_stocks - stocks + int(consumption.item<double>());
                auto returns = reward + gamma * net->predict_values(next_state).detach();

                train(state, action, consumption, returns);
            }

            auto pred_actions = net->predict_actions(next_state);
            torch::Tensor next_action = pred_actions.at(0);
            torch::Tensor next_cons = pred_actions.at(1);
            
            // TODO
            //   - state dostanu jako current state (proste veci z marketu atd)

            history.push_back(std::make_tuple<>(next_state, next_action, next_cons));
            prev_money = money;
            prev_stocks = stocks;

            return trequest();
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
            auto action_probs = net->action_log_prob(actions, consumption, pred_actions);
            auto action_loss = -(advantages.detach() * action_probs).mean(); //TODO minus správně?
            
            auto loss = value_loss + action_loss; //TODO entropy regularization

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
        std::vector<std::tuple<torch::Tensor, torch::Tensor, torch::Tensor>> history;
        std::unique_ptr<TNet> net;
        std::unique_ptr<torch::optim::SGD> optimizer;
    };
}