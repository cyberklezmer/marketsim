#include <torch/torch.h>
#include "nets/proba.hpp"


namespace marketsim {
    template<int state_size, int hidden_size>
    struct ActorCriticBase : public torch::nn::Module {
    public:
        ActorCriticBase()
        {
            linear_critic = register_module("linear_critic", torch::nn::Linear(state_size, hidden_size));
            linear_actor = register_module("linear_actor", torch::nn::Linear(state_size, hidden_size));
            critic = register_module("critic", torch::nn::Linear(hidden_size, 1));
        }

        virtual ~ActorCriticBase() {}

        virtual std::vector<torch::Tensor> predict_actions(const torch::Tensor& x) = 0;
        virtual std::vector<torch::Tensor> predict_actions(torch::Tensor x, bool sample) = 0;

        torch::Tensor predict_values(torch::Tensor x) {
            x = torch::relu(linear_critic->forward(x));
            return torch::tanh(critic->forward(x));
        }

        std::pair<std::vector<torch::Tensor>, torch::Tensor> forward(torch::Tensor x) {            
            auto actions = predict_actions(x, false);
            auto state_values = predict_values(x);
            return std::make_pair<>(actions, state_values);
        }

        virtual torch::Tensor action_log_prob(const torch::Tensor& actions, const torch::Tensor& consumption,
                                              const std::vector<torch::Tensor>& pred_actions) = 0;

        torch::nn::Linear linear_critic{nullptr}, linear_actor{nullptr}, critic{nullptr};
    };

    template<int state_size, int hidden_size, int action_scale>
    struct ACContinuous : ActorCriticBase<state_size, hidden_size> {
        ACContinuous() :
            ActorCriticBase<state_size, hidden_size>(),
            action_size(2)
        {
            //TODO state_size?
            actor_mu = this->register_module("actor_mu", torch::nn::Linear(hidden_size, action_size));
            actor_std = this->register_module("actor_std", torch::nn::Linear(hidden_size, action_size));
            consumption_mu = this->register_module("consumption_mu", torch::nn::Linear(hidden_size, 1));
            consumption_std = this->register_module("consumption_std", torch::nn::Linear(hidden_size, 1));
        }

        std::vector<torch::Tensor> predict_actions(torch::Tensor x, bool sample) {
            x = torch::relu(this->linear_actor->forward(x));
            
            auto mu = torch::tanh(actor_mu->forward(x)) * action_scale;
            auto std = torch::nn::functional::softplus(actor_std->forward(x));

            auto cons_mu = torch::nn::functional::softplus(consumption_mu->forward(x));
            auto cons_std = torch::nn::functional::softplus(consumption_std->forward(x));
            
            auto res = std::vector<torch::Tensor>{mu, std, cons_mu, cons_std};
            if (!sample) {
                return res;
            }

            return sample_actions(res);
        }

        std::vector<torch::Tensor> predict_actions(const torch::Tensor& x) {
            return predict_actions(x, true);
        }

        std::vector<torch::Tensor> sample_actions(std::vector<torch::Tensor> pred_actions) {
            auto mus = pred_actions.at(0);
            auto stds = pred_actions.at(1);
            
            auto cons_mus = pred_actions.at(2);
            auto cons_stds = pred_actions.at(3);

            auto action_sample = normal_sample(mus, stds);
            auto cons_sample = normal_sample(cons_mus, cons_stds);

            return std::vector<torch::Tensor>{action_sample, cons_sample};
        }

        torch::Tensor action_log_prob(const torch::Tensor& actions, const torch::Tensor& consumption,
                                      const std::vector<torch::Tensor>& pred_actions) {

            auto mus = pred_actions.at(0);
            auto stds = pred_actions.at(1);
            
            auto cons_mus = pred_actions.at(2);
            auto cons_stds = pred_actions.at(3);
            
            auto action_proba = normal_log_proba(actions, mus, stds);
            auto cons_proba = normal_log_proba(consumption, cons_mus, cons_stds);

            return action_proba.sum(/*dim=*/1) + cons_proba;
        }

        int action_size;
        torch::nn::Linear actor_mu{nullptr}, actor_std{nullptr}, consumption_mu{nullptr}, consumption_std{nullptr};
    };
}