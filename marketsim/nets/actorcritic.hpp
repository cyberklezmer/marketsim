#include <torch/torch.h>

namespace marketsim {

    struct ActorCriticNet : torch::nn::Module {
        ActorCriticNet() :
            state_size(20),
            action_size(3),
            hidden_size(128)
        {
            //TODO state_size?
            linear = register_module("linear", torch::nn::Linear(state_size, hidden_size));
            critic = register_module("critic", torch::nn::Linear(hidden_size, 1));
            actor_mu = register_module("actor_mu", torch::nn::Linear(hidden_size, action_size));
            actor_std = register_module("actor_std", torch::nn::Linear(hidden_size, action_size));

        }

        std::pair<torch::Tensor, torch::Tensor> predict_actions(torch::Tensor x, bool linear) {
            if (linear) {
                x = pred_linear(x);
            }
            auto mu = actor_mu->forward(x);
            auto std = actor_std->forward(x);
            return std::make_pair<>(mu, std);
        }

        torch::Tensor predict_values(torch::Tensor x, bool linear) {
            if (linear) {
                x = pred_linear(x);
            }
            return critic->forward(x);
        }

        torch::Tensor pred_linear(torch::Tensor x) {
            return torch::relu(linear->forward(x));
        }

        std::tuple<torch::Tensor, torch::Tensor, torch::Tensor> forward(torch::Tensor x) {
            x = pred_linear(x);
            
            auto actions = predict_actions(x, false);
            auto action_mu = std::get<0>(actions);
            auto action_std = std::get<1>(actions);
            auto state_values = predict_values(x, false);
            return std::make_tuple<>(action_mu, action_std, state_values);
        }

        int state_size;
        int hidden_size;
        int action_size;  //TODO zjistit jak consumption atd (viz ten clanek)
        torch::nn::Linear linear{nullptr}, critic{nullptr};
        torch::nn::Linear actor_mu{nullptr}, actor_std{nullptr};
    };
}