#ifndef ACTIONS_HPP_
#define ACTIONS_HPP_

#include <torch/torch.h>
#include "nets/proba.hpp"


namespace marketsim {
    using action_tensors = std::vector<torch::Tensor>;
    using ActFunc = torch::Tensor(const torch::Tensor&);

    template <typename T>
    class action_container {
    public:
        action_container(T bid, T ask, T cons) : bid(bid), ask(ask), cons(cons), flag_valid(false) {}
        action_container(T bid, T ask, T cons, T bid_flag, T ask_flag) :
            bid(bid), ask(ask), cons(cons), bid_flag(bid_flag), ask_flag(ask_flag), flag_valid(true) {}
        
        bool is_flag_valid() const {
            return flag_valid;
        }

        T bid, ask, cons;
        T bid_flag, ask_flag;
    private:
        bool flag_valid;
    };

    template <ActFunc MuActiv, ActFunc StdActiv, int hidden_size, int action_size, int output_scale = 1, int mu_scale = 1>
    class ContinuousActions : public torch::nn::Module {
    public:
        ContinuousActions() {
            actor_mu = this->register_module("actor_mu", torch::nn::Linear(hidden_size, action_size));
            actor_std = this->register_module("actor_std", torch::nn::Linear(hidden_size, action_size));
        }

        action_tensors predict_actions(torch::Tensor x) {
            auto mu = MuActiv(actor_mu->forward(x)) * mu_scale;
            auto std = StdActiv(actor_std->forward(x));

            return action_tensors({mu, std});
        }

        torch::Tensor sample_actions(const action_tensors& pred_actions) {
            torch::Tensor mus = pred_actions.at(0);
            torch::Tensor stds = pred_actions.at(1);

            return normal_sample(mus, stds) * output_scale;
        }

        torch::Tensor action_log_prob(torch::Tensor true_actions, const action_tensors& pred_actions) {
            torch::Tensor mus = pred_actions.at(0);
            torch::Tensor stds = pred_actions.at(1);

            return normal_log_proba(true_actions / output_scale, mus, stds).sum(/*dim=*/1);
        }

        torch::Tensor entropy(const action_tensors& pred_actions) {
            torch::Tensor stds = pred_actions.at(1);
            return normal_entropy(stds);
        }

    private:
        torch::nn::Linear actor_mu{nullptr}, actor_std{nullptr};
    };


    template <int hidden_size, int action_size, int action_offset = 0, int action_mult = 1>
    class DiscreteActions : public torch::nn::Module {
    public:
        DiscreteActions() {
            actor = this->register_module("actor", torch::nn::Linear(hidden_size, action_size));
        }

        action_tensors predict_actions(torch::Tensor x) {
            x = torch::log_softmax(actor->forward(x), /*dim=*/1);
            return action_tensors({x});
        }

        torch::Tensor sample_actions(const action_tensors& pred_actions) {
            torch::Tensor pred = torch::exp(pred_actions.at(0));
            return sample_from_pb(pred) * action_mult - action_offset;
        }

        torch::Tensor action_log_prob(torch::Tensor true_actions, const action_tensors& pred_actions) {
            torch::Tensor logits = pred_actions.at(0);
            true_actions = true_actions.to(torch::kLong);
            true_actions = true_actions / action_mult + action_offset;

            return -torch::nll_loss(logits, true_actions);
        }

        torch::Tensor entropy(const action_tensors& pred_actions) {
            torch::Tensor logits = pred_actions.at(0);
            return logit_entropy(logits);
        }

    private:
        torch::nn::Linear actor{nullptr};
    };

    template <int hidden_size>
    class BinaryAction : public torch::nn::Module {
    public:
        BinaryAction() {
            actor = this->register_module("actor", torch::nn::Linear(hidden_size, 1));
        }

        action_tensors predict_actions(torch::Tensor x) {
            x = actor->forward(x);
            return action_tensors({x});
        }

        torch::Tensor sample_actions(const action_tensors& pred_actions) {
            torch::Tensor pred = torch::sigmoid(pred_actions.at(0));
            return sample_from_pb_binary(pred);
        }
         
        torch::Tensor action_log_prob(torch::Tensor true_actions, const action_tensors& pred_actions) {
            torch::Tensor logits = pred_actions.at(0);
            true_actions = true_actions.to(torch::kLong);

            return -torch::binary_cross_entropy_with_logits(logits, true_actions);
        }

        torch::Tensor entropy(const action_tensors& pred_actions) {
            torch::Tensor logits = pred_actions.at(0);
            torch::Tensor probs = torch::sigmoid(logits);

            return torch::binary_cross_entropy_with_logits(logits, probs);
        }

    private:
        torch::nn::Linear actor{nullptr};
    };

}

#endif // ACTIONS_HPP_