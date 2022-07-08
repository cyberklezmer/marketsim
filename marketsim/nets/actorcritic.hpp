#include <torch/torch.h>
#include "nets/proba.hpp"
#include "nets/actions.hpp"
using namespace torch::indexing;  


namespace marketsim {
    torch::Tensor state_forward(torch::nn::LSTM layer, torch::Tensor x) {
        x = x.view({x.size(0), 1, -1});

        auto res = layer->forward(x);
        torch::Tensor out = std::get<0>(res).index({Slice(), 0});
        
        // get the last output
        auto out_size = out.sizes()[0];
        if (out_size != 1) {
            out = out[out_size - 1].view({1, -1});
        }
        return out;
    }

    torch::Tensor state_forward(torch::nn::Linear layer, torch::Tensor x) {
        return layer->forward(x);
    } 


    template<int state_size, int hidden_size, typename TLayer>
    class ActorCriticBase : public torch::nn::Module {
    public:
        ActorCriticBase()
        {
            state_critic = register_module("state_critic", TLayer(state_size, hidden_size));
            state_actor = register_module("state_actor", TLayer(state_size, hidden_size));
            critic = register_module("critic", torch::nn::Linear(hidden_size, 1));
        }

        virtual ~ActorCriticBase() {}
        
        virtual action_container<torch::Tensor> predict_actions(torch::Tensor x) = 0;
        virtual action_container<action_tensors> predict_actions_train(torch::Tensor x) = 0;

        torch::Tensor predict_values(torch::Tensor x) {
            x = state_forward(this->state_critic, x);
            x = torch::relu(x);
            return critic->forward(x);
        }

        std::pair<action_container<action_tensors>, torch::Tensor> forward(torch::Tensor x) {            
            auto actions = predict_actions_train(x);
            auto state_values = predict_values(x);
            return std::make_pair<>(actions, state_values);
        }

        virtual torch::Tensor action_log_prob(const action_container<torch::Tensor>& actions,
                                              const action_container<action_tensors>& pred) = 0;
        virtual torch::Tensor entropy(const action_container<action_tensors>& pred) = 0;

    protected:
        torch::nn::Linear critic{nullptr};
        TLayer state_critic{nullptr}, state_actor{nullptr};
    };

    template <int state_size, int hidden_size, typename TLayer, typename TBidActor, typename TAskActor, typename TConsActor>
    class ActorCritic : public ActorCriticBase<state_size, hidden_size, TLayer> {
    public:
        ActorCritic() : ActorCriticBase<state_size, hidden_size, TLayer>() {
            bid_actor = this->register_module("bid_actor", TBidActor());
            ask_actor = this->register_module("ask_actor", TAskActor());
            cons_actor = this->register_module("cons_actor", TConsActor());
        }

        virtual action_container<torch::Tensor> predict_actions(torch::Tensor x) {
            auto res = predict_actions_train(x);
            return action_container<torch::Tensor>(
                bid_actor.sample_actions(res.bid),
                ask_actor.sample_actions(res.ask),
                cons_actor.sample_actions(res.cons)
            );
        }

        virtual action_container<action_tensors> predict_actions_train(torch::Tensor x) {
            x = state_forward(this->state_actor, x);
            x = torch::relu(x);

            return action_container<action_tensors>(
                bid_actor.predict_actions(x),
                ask_actor.predict_actions(x),
                cons_actor.predict_actions(x)
            );
        }

        virtual torch::Tensor action_log_prob(const action_container<torch::Tensor>& actions,
                                              const action_container<action_tensors>& pred) {
            torch::Tensor res = bid_actor.action_log_prob(actions.bid, pred.bid);
            res += ask_actor.action_log_prob(actions.ask, pred.ask);
            res += cons_actor.action_log_prob(actions.cons, pred.cons);
            return res;
        }

        virtual torch::Tensor entropy(const action_container<action_tensors>& pred) {
            torch::Tensor res = bid_actor.entropy(pred.bid);
            res += ask_actor.entropy(pred.ask);
            res += cons_actor.entropy(pred.cons);
            return res;
        }

    protected:
        TBidActor bid_actor{nullptr};
        TAskActor ask_actor{nullptr};
        TConsActor cons_actor{nullptr};
    };

    template <int state_size, int hidden_size, typename TLayer, typename TBidActor, typename TAskActor, typename TConsActor>
    class ActorCriticFlags : public ActorCritic<state_size, hidden_size, TLayer, TBidActor, TAskActor, TConsActor> {
    public:
        using Base = ActorCritic<state_size, hidden_size, TLayer, TBidActor, TAskActor, TConsActor>;

        ActorCriticFlags() : ActorCritic<state_size, hidden_size, TLayer, TBidActor, TAskActor, TConsActor>() {
            bid_flag = this->register_module("bid_flag", BinaryAction<hidden_size>());
            ask_flag = this->register_module("ask_flag", BinaryAction<hidden_size>());
        }

        virtual action_container<torch::Tensor> predict_actions(torch::Tensor x) {
            auto pred = Base::predict_actions(x);
            
            action_tensors bid_pred = bid_flag.predict_actions(x);
            action_tensors ask_pred = ask_flag.predict_actions(x);
            return action_container<torch::Tensor>(pred.bid, pred.ask, pred.cons, bid_pred, ask_pred);
        }

        virtual action_container<action_tensors> predict_actions_train(torch::Tensor x) {
            x = state_forward(this->state_actor, x);
            x = torch::relu(x);

            return action_container<action_tensors>(
                this->bid_actor.predict_actions(x),
                this->ask_actor.predict_actions(x),
                this->cons_actor.predict_actions(x),
                this->bid_flag.predict_actions(x),
                this->ask_flag.predict_actions(x)
            );
        }

        virtual torch::Tensor action_log_prob(const action_container<torch::Tensor>& actions,
                                              const action_container<action_tensors>& pred) {
            torch::Tensor log_prob = Base::action_log_prob(actions, pred);
            log_prob += bid_flag.action_log_prob(actions.bid_flag, pred.bid_flag);
            log_prob += ask_flag.action_log_prob(actions.ask_flag, pred.ask_flag);
            return log_prob;
        }

        virtual torch::Tensor entropy(const action_container<action_tensors>& pred) {
            torch::Tensor res = Base::entropy(pred);
            res += bid_flag.entropy(pred.bid_flag);
            res += ask_flag.entropy(pred.ask_flag);
            return res;
        }

    private:
        BinaryAction<hidden_size> bid_flag, ask_flag;
    };
}