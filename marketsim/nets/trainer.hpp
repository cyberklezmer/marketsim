#include "marketsim.hpp"
#include <torch/torch.h>

namespace marketsim {

    using hist_entry = std::tuple<torch::Tensor, torch::Tensor, torch::Tensor>;

    class BaseTrainer {
    public:
        virtual ~BaseTrainer() {}

    private:
        virtual void train(const std::vector<hist_entry> &history, torch::Tensor next_state) = 0;

        virtual std::vector<torch::Tensor> predict_actions(const torch::Tensor& state) = 0;
    };

    template<typename TNet>
    class NStepTrainer : BaseTrainer {
    public:
        NStepTrainer() : BaseTrainer(), net(std::make_unique<TNet>())
        {
            optimizer = std::make_unique<torch::optim::Adam>(net->parameters(), /*lr=*/0.001);
        }

    private:
        std::unique_ptr<torch::optim::Adam> optimizer;
        std::unique_ptr<TNet> net;
    };

}

/*
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

*/