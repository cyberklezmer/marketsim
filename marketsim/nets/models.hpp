#ifndef NET_MODELS_HPP_
#define NET_MODELS_HPP_

#include <torch/torch.h>
#include "nets/utils.hpp"


namespace marketsim {

    template<typename TActor, typename TCritic>
    class ActorCritic {
    public:
        ActorCritic() :
            clamp(2), beta(0.01)
        {
            actor = std::make_shared<TActor>();
            critic = std::make_shared<TCritic>();

            actor_opt = std::make_unique<torch::optim::Adam>(actor->parameters(), /*lr=*/0.001);
            critic_opt = std::make_unique<torch::optim::Adam>(critic->parameters(), /*lr=*/0.0001);
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

#endif  //NET_MODELS_HPP_