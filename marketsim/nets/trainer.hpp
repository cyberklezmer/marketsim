#include "marketsim.hpp"
#include <torch/torch.h>
#include "nets/utils.hpp"
#include "nets/actions.hpp"


namespace marketsim {

    class BaseTrainer {
    public:
        virtual ~BaseTrainer() {}

    private:
        virtual void train(const std::vector<hist_entry>& history, torch::Tensor next_state) = 0;

        virtual action_container<torch::Tensor> predict_actions(const std::vector<hist_entry>& history, torch::Tensor state) = 0;
    };

    template<typename TNet, int N, typename TReturns, bool use_entropy = false, bool stack = false, int stack_dim = 0, int stack_size = 5, int clamp = 2>
    class NStepTrainer : BaseTrainer {
    public:
        NStepTrainer() : BaseTrainer(), net(std::make_unique<TNet>()), returns_func(), beta(0.01)
        {
            optimizer = std::make_unique<torch::optim::Adam>(net->parameters(), /*lr=*/0.0001);
        }

        void train(const std::vector<hist_entry>& history, torch::Tensor next_state) {
            int hist_min = stack ? stack_size + 1 : 0;
            if (history.size() < N + hist_min) {
                return;
            }

            // get data for training
            size_t idx = history.size() - N;

            auto curr = history.at(idx);
            torch::Tensor state = curr.state;
            auto actions = curr.actions;
            torch::Tensor returns = compute_returns(history, next_state);
            
            if (stack) {
                auto mod_states = get_modified_states(history, N + 1, stack_size);
                adjust_state_size(mod_states);
                state = stack_states(mod_states);
            }
            else {
                state = modify_state(state);
            }
            
            // main loop
            optimizer->zero_grad();

            auto pred = net->forward(state);
            auto pred_actions = std::get<0>(pred);
            auto state_value = std::get<1>(pred);

            auto advantages = (returns - state_value);
            auto entropy = net->entropy(pred_actions);

            auto value_loss = advantages.pow(2).mean();
            //auto value_loss = torch::huber_loss(state_value, returns);
            auto action_probs = net->action_log_prob(actions, pred_actions);
            auto action_loss = -(advantages.detach() * action_probs + beta * entropy).mean(); //TODO minus správně?

            //action_loss = torch::clamp(action_loss, -2, 2);
            
            auto loss = value_loss + action_loss;
            loss = torch::clamp(loss, -clamp, clamp);

            loss.backward();
            optimizer->step();
        }
    
        action_container<torch::Tensor> predict_actions(const std::vector<hist_entry>& history, torch::Tensor state) {
            torch::NoGradGuard no_grad;

            if (stack) {
                auto mod_states = get_modified_states(history, state, stack_size);
                adjust_state_size(mod_states);
                state = stack_states(mod_states);
            }
            else {
                state = modify_state(state);
            }
            return net->predict_actions(state);
        }

    private:
        torch::Tensor stack_states(std::vector<torch::Tensor> states) {
            return torch::cat(states, stack_dim);
        }

        void adjust_state_size(std::vector<torch::Tensor>& states) {  
            if (stack_dim == 1) {
                while (states.size() != stack_size) {
                    states.insert(states.begin(), states.at(0));
                }
            }
        }

        std::vector<torch::Tensor> get_modified_states(const std::vector<hist_entry>& history, torch::Tensor state, int st_size) {
            auto states = get_modified_states(history, 0, st_size - 1);
            states.push_back(modify_state(state));
            return states;
        }

        std::vector<torch::Tensor> get_modified_states(const std::vector<hist_entry>& history, int start, int st_size) {
            int size = std::min(int(history.size()), st_size);
            auto past = get_past_states(history, start, size);

            // modify states in the vector
            std::vector<torch::Tensor> modified_past;
            std::transform(past.begin(), past.end(), std::back_inserter(modified_past),
                           [&](torch::Tensor t) { return this->modify_state(t); });

            return modified_past;
        }

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

            if (!stack || stack_dim != 1) {
                tensor_returns += gamma * net->predict_values(modify_state(next_state));
            }
            return tensor_returns;
        }

        double beta;
        std::unique_ptr<torch::optim::Adam> optimizer;
        std::unique_ptr<TNet> net;
        TReturns returns_func;
    };

}
