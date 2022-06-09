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

        virtual std::vector<torch::Tensor> predict_actions(torch::Tensor x) = 0;
        virtual std::vector<torch::Tensor> predict_actions(torch::Tensor x, bool sample) = 0;
        virtual std::vector<torch::Tensor> sample_actions(const std::vector<torch::Tensor>& pred_actions) = 0;


        torch::Tensor predict_values(torch::Tensor x) {
            x = torch::relu(linear_critic->forward(x));
            return critic->forward(x);
        }

        std::pair<std::vector<torch::Tensor>, torch::Tensor> forward(torch::Tensor x) {            
            auto actions = predict_actions(x, false);
            auto state_values = predict_values(x);
            return std::make_pair<>(actions, state_values);
        }

        virtual torch::Tensor action_log_prob(torch::Tensor actions, torch::Tensor consumption,
                                              const std::vector<torch::Tensor>& pred_actions) = 0;

        virtual torch::Tensor entropy(const std::vector<torch::Tensor>& pred_actions) = 0;

        torch::nn::Linear linear_critic{nullptr}, linear_actor{nullptr}, critic{nullptr};
    };

    template<int state_size, int hidden_size, int action_scale, int cons_mult>
    struct ACContinuous : ActorCriticBase<state_size, hidden_size> {
        ACContinuous() :
            ActorCriticBase<state_size, hidden_size>(),
            action_size(2)
        {
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

        std::vector<torch::Tensor> predict_actions(torch::Tensor x) {
            return predict_actions(x, true);
        }

        std::vector<torch::Tensor> sample_actions(const std::vector<torch::Tensor>& pred_actions) {
            auto mus = pred_actions.at(0);
            auto stds = pred_actions.at(1);
            
            auto cons_mus = pred_actions.at(2);
            auto cons_stds = pred_actions.at(3);

            auto action_sample = normal_sample(mus, stds);
            auto cons_sample = normal_sample(cons_mus, cons_stds) * cons_mult;

            return std::vector<torch::Tensor>{action_sample, cons_sample};
        }

        torch::Tensor action_log_prob(torch::Tensor actions, torch::Tensor consumption,
                                      const std::vector<torch::Tensor>& pred_actions) {

            auto mus = pred_actions.at(0);
            auto stds = pred_actions.at(1);
            
            auto cons_mus = pred_actions.at(2);
            auto cons_stds = pred_actions.at(3);
            
            auto action_proba = normal_log_proba(actions, mus, stds);
            auto cons_proba = normal_log_proba(consumption / cons_mult, cons_mus, cons_stds);

            return action_proba.sum(/*dim=*/1) + cons_proba;
        }

        torch::Tensor entropy(const std::vector<torch::Tensor>& pred_actions) {
            auto stds = pred_actions.at(1);
            auto cons_stds = pred_actions.at(3);

            return normal_entropy(stds) + normal_entropy(cons_stds);
        }

        int action_size;
        torch::nn::Linear actor_mu{nullptr}, actor_std{nullptr}, consumption_mu{nullptr}, consumption_std{nullptr};
    };

    template <int state_size, int hidden_size, int action_h, int cons_max, int cons_div>
    struct ACDiscrete : ActorCriticBase<state_size, hidden_size> {
        ACDiscrete() : 
            ActorCriticBase<state_size, hidden_size>() {
                int ba_size = action_h * 2 + 1;
                cons_step_size = int(std::ceil(cons_max / cons_div));

                bid_actor = this->register_module("bid_actor", torch::nn::Linear(hidden_size, ba_size));
                ask_actor = this->register_module("ask_actor", torch::nn::Linear(hidden_size, ba_size));
                cons_actor = this->register_module("cons_actor", torch::nn::Linear(hidden_size, cons_div + 1));
            }

        std::vector<torch::Tensor> predict_actions(torch::Tensor x) {
            return predict_actions(x, true);
        }

        std::vector<torch::Tensor> predict_actions(torch::Tensor x, bool sample) {
             x = torch::relu(this->linear_actor->forward(x));
            
            auto bid = torch::log_softmax(bid_actor->forward(x), /*dim=*/1);
            auto ask = torch::log_softmax(ask_actor->forward(x), /*dim=*/1);
            auto cons = torch::log_softmax(cons_actor->forward(x), /*dim=*/1);

            auto res = std::vector<torch::Tensor>{bid, ask, cons};
            if (!sample) {
                return res;
            }

            return sample_actions(res);
        }

        std::vector<torch::Tensor> sample_actions(const std::vector<torch::Tensor>& pred_actions) {
            auto bid_logits = at::exp(pred_actions.at(0));
            auto ask_logits = at::exp(pred_actions.at(1));
            auto cons_logits = at::exp(pred_actions.at(2));

            torch::Tensor bid = sample_from_pb(bid_logits) - action_h;
            torch::Tensor ask = sample_from_pb(ask_logits) - action_h;
            torch::Tensor cons = sample_from_pb(cons_logits) * cons_step_size;

            std::vector<torch::Tensor> res{
                at::concat({bid, ask}).reshape({1, 2}),
                cons.reshape({1, 1})
            };

            return res;
        }

        torch::Tensor action_log_prob(torch::Tensor actions, torch::Tensor consumption,
                                      const std::vector<torch::Tensor>& pred_actions) {
            auto bid_logits = pred_actions.at(0);
            auto ask_logits = pred_actions.at(1);
            auto cons_logits = pred_actions.at(2);

            torch::Tensor bid_target = (actions[0][0].reshape({1}) + action_h).to(torch::kLong);
            torch::Tensor ask_target = (actions[0][1].reshape({1}) + action_h).to(torch::kLong);
            torch::Tensor cons_target = (consumption[0] / cons_step_size).to(torch::kLong);

            torch::Tensor loss = torch::nll_loss(bid_logits, bid_target);
            loss += torch::nll_loss(ask_logits, ask_target);
            loss += torch::nll_loss(cons_logits, cons_target);

            return -loss;
        }
        
        torch::Tensor entropy(const std::vector<torch::Tensor>& pred_actions) {
            auto bid_logits = pred_actions.at(0);
            auto ask_logits = pred_actions.at(1);
            auto cons_logits = pred_actions.at(2);

            return logit_entropy(bid_logits) + logit_entropy(ask_logits) + logit_entropy(cons_logits);
        }

        int cons_step_size;
        torch::nn::Linear bid_actor{nullptr}, ask_actor{nullptr}, cons_actor{nullptr};
    };
}