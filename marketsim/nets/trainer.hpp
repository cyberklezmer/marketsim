#include "marketsim.hpp"
#include <torch/torch.h>

namespace marketsim {

    using hist_entry = std::tuple<torch::Tensor, torch::Tensor, torch::Tensor>;

    class BaseTrainer {
    public:
        virtual ~BaseTrainer() {}

    private:
        virtual void train(const std::vector<hist_entry>& history, const torch::Tensor& next_state) = 0;

        virtual std::vector<torch::Tensor> predict_actions(const torch::Tensor& state) = 0;
    };

    template<typename TNet, int N>
    class NStepTrainer : BaseTrainer {
    public:
        NStepTrainer() : BaseTrainer(), net(std::make_unique<TNet>()), gamma(0.99998)
        {
            optimizer = std::make_unique<torch::optim::Adam>(net->parameters(), /*lr=*/0.0001);
        }

        virtual void train(const std::vector<hist_entry>& history, const torch::Tensor& next_state) {
            if (history.size() < N) {
                return;
            }

            // get data for training
            size_t idx = history.size() - N;

            auto curr = history.at(idx);
            torch::Tensor state = std::get<0>(curr);
            torch::Tensor actions = std::get<1>(curr);
            torch::Tensor cons = std::get<2>(curr);
            torch::Tensor returns = compute_returns(history, next_state);
            //std::cout << "\nReturns " << returns.item<double>() << std::endl;
            //std::cout << "State: " << state << std::endl;

            state = modify_state(state);

            // main loop
            optimizer->zero_grad();

            auto pred = net->forward(state);
            auto pred_actions = std::get<0>(pred);
            auto state_value = std::get<1>(pred);

            auto advantages = (returns - state_value);

            auto value_loss = advantages.pow(2).mean();
            //auto value_loss = at::huber_loss(state_value, returns);
            auto action_probs = net->action_log_prob(actions, cons, pred_actions);
            auto action_loss = -(advantages.detach() * action_probs).mean(); //TODO minus správně?

            //action_loss = at::clamp(action_loss, -2, 2);
            
            auto loss = value_loss + action_loss; //TODO entropy regularization
            loss = at::clamp(loss, -2, 2);

            //std::cout << "Value " << value_loss << "Action: " << action_loss << std::endl;
            //std::cout << "State_value: " << state_value << std::endl;

            loss.backward();
            optimizer->step();
        }
    
    virtual std::vector<torch::Tensor> predict_actions(const torch::Tensor& state) {
        torch::NoGradGuard no_grad;
        torch::Tensor x = modify_state(state);
        return net->predict_actions(x);
    }

    private:
        torch::Tensor modify_state(torch::Tensor x) {
            x[0][0] /= 1000;
            for (int i = 1; i <= 3; ++i) {
                x[0][i] /= 10000;
            }

            return x;
        }

        torch::Tensor compute_returns(const std::vector<hist_entry>& history, const torch::Tensor& next_state) {
            double curr_gamma = 1.0;
            double returns = 0.0;
            size_t hist_size = history.size();

            for (int i = hist_size - N; i < hist_size; ++i) {
                auto entry = history.at(i);
                torch::Tensor next = i < hist_size - 1 ? std::get<0>(history.at(i + 1)) : next_state;

                double cons =  std::get<2>(entry).item<double>();
                double rew = get_reward_diff(std::get<0>(entry), next);

                //std::cout << "\nConsumption: " << cons << "Reward: " << rew << std::endl;
                returns += curr_gamma * (cons + rew);
                curr_gamma *= gamma;
            }

            //std::cout << "\n\nReturns without estimate: " << returns << "Gamma: " << curr_gamma << std::endl;

            auto tensor_returns = torch::tensor({returns}).reshape({1,1});
            tensor_returns += curr_gamma * net->predict_values(modify_state(next_state));  //TODO preprocess state?
            return tensor_returns;
        }

        double get_reward_diff(const torch::Tensor& state, const torch::Tensor& next_state) {
            //torch::Tensor state = modify_state(s);
            //torch::Tensor next_state = modify_state(ns);

            double mdiff = next_state[0][0].item<double>() - state[0][0].item<double>();
            
            double next_stock = next_state[0][3].item<double>() * next_state[0][1].item<double>();
            double stock = state[0][3].item<double>() * state[0][1].item<double>();
            double sdiff = next_stock - stock;

            return mdiff + sdiff;
        }

        double gamma;
        std::unique_ptr<torch::optim::Adam> optimizer;
        std::unique_ptr<TNet> net;
    };

}
