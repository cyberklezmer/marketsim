#ifndef MSNEURAL_ACTIONS_HPP_
#define MSNEURAL_ACTIONS_HPP_

#include <torch/torch.h>
#include "msneural/config.hpp"
#include "msneural/proba.hpp"


namespace marketsim {
    using action_tensors = std::vector<torch::Tensor>;
    using TensorFunc = torch::Tensor(const torch::Tensor&);

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

    action_container<torch::Tensor> actions_batch(const std::vector<action_container<torch::Tensor>>& actions) {
        std::vector<torch::Tensor> bv;
        std::vector<torch::Tensor> av;
        std::vector<torch::Tensor> cv;
        std::vector<torch::Tensor> bfv;
        std::vector<torch::Tensor> afv;

        for (auto const& a : actions) {
            bv.push_back(a.bid);
            av.push_back(a.ask);
            cv.push_back(a.cons);

            if (a.is_flag_valid()) {
                bfv.push_back(a.bid_flag);
                afv.push_back(a.ask_flag);
            }
        }

        assert(bfv.size() == 0 || bfv.size() == bv.size());
        
        auto stack_tensors = [](const std::vector<torch::Tensor>& tensors){ return torch::cat(tensors, 0); };

        if (bfv.size() > 0) {
            return action_container<torch::Tensor>(stack_tensors(bv), stack_tensors(av), stack_tensors(cv),
                                                   stack_tensors(bfv), stack_tensors(afv));
        }

        return action_container<torch::Tensor>(stack_tensors(bv), stack_tensors(av), stack_tensors(cv));
    }

    template <TensorFunc func>
    action_container<torch::Tensor> actions_map(const action_container<torch::Tensor>& actions) {
        auto bid = func(actions.bid);
        auto ask = func(actions.ask);
        auto cons = func(actions.cons);

        if (actions.is_flag_valid()) {
            auto bid_flag = func(actions.bid_flag); 
            auto ask_flag = func(actions.ask_flag);
            return action_container<torch::Tensor>(bid, ask, cons, bid_flag, ask_flag);
        }
        
        return action_container<torch::Tensor>(bid, ask, cons);
    }

    template <int... size>
    torch::Tensor tensor_view(const torch::Tensor& t) {
        return t.view({size...});
    }

    template <int dim>
    torch::Tensor tensor_unsqueeze(const torch::Tensor& t) {
        return t.unsqueeze(dim);
    }

    template <TensorFunc MuActiv, TensorFunc StdActiv>
    class ContinuousActions : public torch::nn::Module {
    public:
        ContinuousActions(int hidden_size, action_config cfg) {
            output_scale = cfg.output_scale;
            mu_scale = cfg.mu_scale;

            actor_mu = this->register_module("actor_mu", torch::nn::Linear(hidden_size, cfg.action_size));
            actor_std = this->register_module("actor_std", torch::nn::Linear(hidden_size, cfg.action_size));
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
        double output_scale, mu_scale;

        torch::nn::Linear actor_mu{nullptr}, actor_std{nullptr};
    };

    class DiscreteActions : public torch::nn::Module {
    public:
        DiscreteActions(int hidden_size, action_config cfg) {
            action_offset = cfg.action_offset;
            action_mult = cfg.action_mult;

            actor = this->register_module("actor", torch::nn::Linear(hidden_size, cfg.action_size));
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
            true_actions = true_actions / action_mult + action_offset;
            true_actions = true_actions.to(torch::kLong).flatten();

            return -torch::nll_loss(logits, true_actions);
        }

        torch::Tensor entropy(const action_tensors& pred_actions) {
            torch::Tensor logits = pred_actions.at(0);
            return logit_entropy(logits);
        }

    private:
        double action_offset, action_mult;

        torch::nn::Linear actor{nullptr};
    };

    class BinaryAction : public torch::nn::Module {
    public:
        BinaryAction(int hidden_size) {
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

#endif // MSNEURAL_ACTIONS_HPP_