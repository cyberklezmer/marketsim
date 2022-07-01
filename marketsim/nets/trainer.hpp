#include "marketsim.hpp"
#include <torch/torch.h>

namespace marketsim {

    using hist_entry = std::tuple<torch::Tensor, torch::Tensor, torch::Tensor>;

    class BaseTrainer {
    public:
        virtual ~BaseTrainer() {}

    private:
        virtual void train(const std::vector<hist_entry>& history, torch::Tensor next_state) = 0;

        virtual std::vector<torch::Tensor> predict_actions(torch::Tensor state) = 0;
    };

    template<typename TNet, int N, typename TReturns, bool use_entropy = false, int clamp = 2>
    class NStepTrainer : BaseTrainer {
    public:
        NStepTrainer() : BaseTrainer(), net(std::make_unique<TNet>()), returns_func(), beta(0.01)
        {
            optimizer = std::make_unique<torch::optim::Adam>(net->parameters(), /*lr=*/0.0001);
        }

        void train(const std::vector<hist_entry>& history, torch::Tensor next_state) {
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

            /*TODO yes, but
            // look into the past here!
            // add it into prediction.

            std::vector<torch::Tensor> large_state;
            for (int i = idx; i < history.size(); ++i) {
                auto entry = history.at(i);
                auto s = std::get<0>(entry);
                large_state.push_back(modify_state(s));
            }
            state = modify_state(state);
            large_state.push_back(state);
            state = torch::cat(large_state); */
            
            state = modify_state(state);
            
            // main loop
            optimizer->zero_grad();

            auto pred = net->forward(state);
            auto pred_actions = std::get<0>(pred);
            auto state_value = std::get<1>(pred);

            auto advantages = (returns - state_value);
            auto entropy = net->entropy(pred_actions);

            auto value_loss = advantages.pow(2).mean();
            //auto value_loss = torch::huber_loss(state_value, returns);
            auto action_probs = net->action_log_prob(actions, cons, pred_actions);
            auto action_loss = -(advantages.detach() * action_probs + beta * entropy).mean(); //TODO minus správně?

            //action_loss = torch::clamp(action_loss, -2, 2);
            
            auto loss = value_loss + action_loss;
            loss = torch::clamp(loss, -clamp, clamp);

            loss.backward();
            optimizer->step();
        }
    
        std::vector<torch::Tensor> predict_actions(torch::Tensor state) {
            torch::NoGradGuard no_grad;
            torch::Tensor x = modify_state(state);
            return net->predict_actions(x);
        }

    private:
        torch::Tensor modify_state(torch::Tensor state) {
            torch::Tensor x = state.clone();

            x[0][0] /= 1000;
            for (int i = 1; i <= 3; ++i) {
                x[0][i] /= 10000;
            }

            return x;
        }

        torch::Tensor compute_returns(const std::vector<hist_entry>& history, torch::Tensor next_state) {
            torch::Tensor tensor_returns = returns_func.compute_returns(history, next_state);
            double gamma = returns_func.get_gamma();

            tensor_returns += gamma * net->predict_values(modify_state(next_state));
            return tensor_returns;
        }

        double beta;
        std::unique_ptr<torch::optim::Adam> optimizer;
        std::unique_ptr<TNet> net;
        TReturns returns_func;
    };

}
