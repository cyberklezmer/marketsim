#ifndef MSNEURAL_MODELS_HPP_
#define MSNEURAL_MODELS_HPP_

#include <torch/torch.h>
#include "msneural/utils.hpp"


namespace marketsim {

    std::vector<int> get_indices(int flat_idx, const std::vector<int>& sizes) {
        std::vector<int> res;

        for (auto i = sizes.begin(); i != sizes.end(); ++i) {
            res.push_back(flat_idx % *i);
            flat_idx /= *i;
        }
        
        std::reverse(res.begin(), res.end());
        return res;
    }

    torch::Tensor get_flat_index(const std::vector<torch::Tensor>& idx, const std::vector<int>& sizes) {
        assert(idx.size() == sizes.size());

        torch::Tensor flat_idx = idx.at(0);
        for (size_t i = 1; i < idx.size(); ++i) {
            flat_idx *= sizes.at(i);
            flat_idx += idx.at(i);
        }

        return flat_idx;
    }

    
    template <typename TConfig, typename TCritic>
    class QNet {
    public:
        QNet() : clamp(2), epsilon(0.25) {
            auto cfg = TConfig::config;
            cons_on = cfg->strategy.train_cons;

            sizes.push_back(cfg->actions.action_size);  // bid
            sizes.push_back(cfg->actions.action_size);  // ask
            action_offset = cfg->actions.action_offset;

            if (cons_on) {
                sizes.push_back(cfg->cons.action_size);
                cons_offset = cfg->cons.action_offset;
                cons_mult = cfg->cons.action_mult;
            }
            std::reverse(sizes.begin(), sizes.end());

            action_size = 1;
            for (auto s : sizes) {
                action_size *= s;
            }

            critic = std::make_shared<TCritic>();
            critic_opt = std::make_unique<torch::optim::Adam>(critic->parameters(), /*lr=*/cfg->model.lr);
        }

        void train(const hist_entry& batch) {
            torch::Tensor state = batch.state;
            action_container<torch::Tensor> actions = batch.actions;
            torch::Tensor returns = batch.returns.squeeze(1);
            
            // main loop
            critic_opt->zero_grad();

            torch::Tensor values = critic->forward(state);
            torch::Tensor target_values = predict_values(state, false);

            torch::Tensor idx = get_action_index(actions);
            for (size_t i = 0; i < target_values.sizes()[0]; ++i) {
                target_values[i][idx[i]] = returns[i];
            }

            torch::Tensor loss = (target_values - values).pow(2).mean();
            loss = torch::clamp(loss, -clamp, clamp);

            loss.backward();
            critic_opt->step();
        }
        
        action_container<torch::Tensor> predict_actions(torch::Tensor state) {
            torch::NoGradGuard no_grad;
            
            double rand = torch::rand({1}).item<double>();

            torch::Tensor values = (rand < epsilon) ? torch::tensor({random_int_from_tensor(0, action_size)}) : critic->forward(state);
            values = torch::argmax(values);

            std::vector<int> actions = get_indices(values.item<int>(), sizes);
            
            torch::Tensor bid = torch::tensor({actions.at(0) - action_offset});
            torch::Tensor ask = torch::tensor({actions.at(1) - action_offset});
            int cons = cons_on ? ((actions.at(2) - cons_offset) * cons_mult) : 0;

            return action_container<torch::Tensor>(bid, ask, torch::tensor({cons}));
        }

        torch::Tensor predict_values(torch::Tensor state, bool max = true) {
            torch::NoGradGuard no_grad;

            torch::Tensor values = critic->forward(state);

            if (max) {
                return std::get<0>(values.max(1));
            }
            return values;
        }
    private:
        torch::Tensor get_action_index(action_container<torch::Tensor> actions) {
            std::vector<torch::Tensor> indices{actions.bid + action_offset, actions.ask + action_offset};
            if (cons_on) {
                indices.push_back(actions.cons / cons_mult + cons_offset);
            }

            return get_flat_index(indices, sizes);
        }

        double clamp, epsilon;

        bool cons_on;
        int action_size;
        std::vector<int> sizes;
        int action_offset, cons_offset, cons_mult;

        std::shared_ptr<TCritic> critic{nullptr};
        std::unique_ptr<torch::optim::Adam> critic_opt;
    };

    template<typename TConfig, typename TActor, typename TCritic>
    class ActorCritic {
    public:
        ActorCritic() :
            clamp(2), beta(0.01)
        {           
            auto cfg = TConfig::config;

            actor = std::make_shared<TActor>();
            critic = std::make_shared<TCritic>();

            actor_opt = std::make_unique<torch::optim::Adam>(actor->parameters(), /*lr=*/cfg->model.actor_lr);
            critic_opt = std::make_unique<torch::optim::Adam>(critic->parameters(), /*lr=*/cfg->model.critic_lr);
        }

        void train(const hist_entry& batch) {
            torch::Tensor state = batch.state;
            action_container<torch::Tensor> actions = batch.actions;
            torch::Tensor returns = batch.returns;
            
            // main loop
            actor_opt->zero_grad();
            critic_opt->zero_grad();

            auto pred_actions = actor->predict_actions_train(state);
            torch::Tensor state_value = critic->forward(state);

            auto advantages = (returns - state_value);
            auto entropy = actor->entropy(pred_actions);

            auto value_loss = advantages.pow(2).mean();
            //auto value_loss = torch::huber_loss(state_value, returns);
            auto action_probs = actor->action_log_prob(actions, pred_actions);
            auto action_loss = -(advantages.detach() * action_probs + beta * entropy).mean(); //TODO minus správně?
            //action_loss = torch::clamp(action_loss, -2, 2);
            
            auto loss = value_loss + action_loss;
            loss = torch::clamp(loss, -clamp, clamp);

            loss.backward();
            actor_opt->step();
            critic_opt->step();
        }
        
        action_container<torch::Tensor> predict_actions(torch::Tensor state) {
            torch::NoGradGuard no_grad;
            return actor->predict_actions(state);
        }

        torch::Tensor predict_values(torch::Tensor state) {
            torch::NoGradGuard no_grad;
            return critic->forward(state);
        }

    private:
        std::shared_ptr<TActor> actor{nullptr};
        std::shared_ptr<TCritic> critic{nullptr};

        double beta, clamp;
        std::unique_ptr<torch::optim::Adam> actor_opt, critic_opt;
    };
}

#endif  //MSNEURAL_MODELS_HPP_