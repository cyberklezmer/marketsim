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
            bid_actor = this->register_module("bid_actor", std::make_shared<TBidActor>());
            ask_actor = this->register_module("ask_actor", std::make_shared<TAskActor>());
            cons_actor = this->register_module("cons_actor", std::make_shared<TConsActor>());
        }
        virtual ~ActorCritic() {}

        virtual action_container<torch::Tensor> predict_actions(torch::Tensor x) {
            auto res = predict_actions_train(x);
            return action_container<torch::Tensor>(
                bid_actor->sample_actions(res.bid),
                ask_actor->sample_actions(res.ask),
                cons_actor->sample_actions(res.cons)
            );
        }

        virtual action_container<action_tensors> predict_actions_train(torch::Tensor x) {
            x = state_forward(this->state_actor, x);
            x = torch::relu(x);

            return action_container<action_tensors>(
                bid_actor->predict_actions(x),
                ask_actor->predict_actions(x),
                cons_actor->predict_actions(x)
            );
        }

        virtual torch::Tensor action_log_prob(const action_container<torch::Tensor>& actions,
                                              const action_container<action_tensors>& pred) {
            torch::Tensor res = bid_actor->action_log_prob(actions.bid, pred.bid);
            res += ask_actor->action_log_prob(actions.ask, pred.ask);
            res += cons_actor->action_log_prob(actions.cons, pred.cons);
            return res;
        }

        virtual torch::Tensor entropy(const action_container<action_tensors>& pred) {
            torch::Tensor res = bid_actor->entropy(pred.bid);
            res += ask_actor->entropy(pred.ask);
            res += cons_actor->entropy(pred.cons);
            return res;
        }

    protected:
        std::shared_ptr<TBidActor> bid_actor{nullptr};
        std::shared_ptr<TAskActor> ask_actor{nullptr};
        std::shared_ptr<TConsActor> cons_actor{nullptr};
    };

    template <int state_size, int hidden_size, typename TLayer, typename TBidActor, typename TAskActor, typename TConsActor>
    class ActorCriticFlags : public ActorCritic<state_size, hidden_size, TLayer, TBidActor, TAskActor, TConsActor> {
    public:
        using Base = ActorCritic<state_size, hidden_size, TLayer, TBidActor, TAskActor, TConsActor>;

        ActorCriticFlags() : ActorCritic<state_size, hidden_size, TLayer, TBidActor, TAskActor, TConsActor>() {
            bid_flag = this->register_module("bid_flag", std::make_shared<BinaryAction<hidden_size>>());
            ask_flag = this->register_module("ask_flag", std::make_shared<BinaryAction<hidden_size>>());
        }

        virtual action_container<torch::Tensor> predict_actions(torch::Tensor x) {
            auto pred = predict_actions_train(x);
            
            return action_container<torch::Tensor>(
                this->bid_actor->sample_actions(pred.bid),
                this->ask_actor->sample_actions(pred.ask), 
                this->cons_actor->sample_actions(pred.cons),
                this->bid_flag->sample_actions(pred.bid_flag),
                this->ask_flag->sample_actions(pred.ask_flag)
            );
        }

        virtual action_container<action_tensors> predict_actions_train(torch::Tensor x) {
            x = state_forward(this->state_actor, x);
            x = torch::relu(x);

            return action_container<action_tensors>(
                this->bid_actor->predict_actions(x),
                this->ask_actor->predict_actions(x),
                this->cons_actor->predict_actions(x),
                this->bid_flag->predict_actions(x),
                this->ask_flag->predict_actions(x)
            );
        }

        virtual torch::Tensor action_log_prob(const action_container<torch::Tensor>& actions,
                                              const action_container<action_tensors>& pred) {
            torch::Tensor log_prob = Base::action_log_prob(actions, pred);
            log_prob += bid_flag->action_log_prob(actions.bid_flag, pred.bid_flag);
            log_prob += ask_flag->action_log_prob(actions.ask_flag, pred.ask_flag);
            return log_prob;
        }

        virtual torch::Tensor entropy(const action_container<action_tensors>& pred) {
            torch::Tensor res = Base::entropy(pred);
            res += bid_flag->entropy(pred.bid_flag);
            res += ask_flag->entropy(pred.ask_flag);
            return res;
        }

    private:
        std::shared_ptr<BinaryAction<hidden_size>> bid_flag, ask_flag;
    };

    template <int state_size, int hidden_size, typename TLayer, typename TConsActor>
    class ActorCriticSpeculator : public ActorCriticBase<state_size, hidden_size, TLayer> {
    public:
        ActorCriticSpeculator() : ActorCriticBase<state_size, hidden_size, TLayer>(), zero_tensor(torch::zeros({1})) {
            bid_ask_actor = this->register_module("bid_ask_actor", std::make_shared<DiscreteActions<hidden_size, 3>>());
            cons_actor = this->register_module("cons_actor", std::make_shared<TConsActor>());
        }
            
         virtual action_container<torch::Tensor> predict_actions(torch::Tensor x) {
            auto pred = predict_actions_train(x);
            torch::Tensor bid_ask = bid_ask_actor->sample_actions(pred.bid);

            double bid = (bid_ask.item<int>() == 1) ? 1.0 : 0.0;
            double ask = (bid_ask.item<int>() == 2) ? 1.0 : 0.0;
            
            return action_container<torch::Tensor>(
                zero_tensor,
                zero_tensor,
                this->cons_actor->sample_actions(pred.cons),
                torch::tensor({bid}),
                torch::tensor({ask})
            );
        }

        virtual action_container<action_tensors> predict_actions_train(torch::Tensor x) {
            x = state_forward(this->state_actor, x);
            x = torch::relu(x);

            return action_container<action_tensors>(
                this->bid_ask_actor->predict_actions(x),
                action_tensors(),
                this->cons_actor->predict_actions(x)
            );
        }

        virtual torch::Tensor action_log_prob(const action_container<torch::Tensor>& actions,
                                              const action_container<action_tensors>& pred) {
            torch::Tensor log_prob = cons_actor->action_log_prob(actions.cons, pred.cons);
            
            torch::Tensor true_target = construct_target(actions);
            log_prob += bid_ask_actor->action_log_prob(true_target, pred.bid);

            return log_prob;
        }

        virtual torch::Tensor entropy(const action_container<action_tensors>& pred) {
            torch::Tensor res = cons_actor->entropy(pred.cons);
            res += bid_ask_actor->entropy(pred.bid);

            return res;
        }
        

    private:
        torch::Tensor construct_target(const action_container<torch::Tensor>& actions) {
            double bid = actions.bid_flag.item<double>();
            double ask = actions.ask_flag.item<double>();

            assert((bid + ask) <= 1.001);
            
            double res = bid + 2 * ask;
            std::cout << res << std::endl;

            return torch::tensor({res});
        }

        torch::Tensor zero_tensor;
        std::shared_ptr<DiscreteActions<hidden_size, 3>> bid_ask_actor;
        std::shared_ptr<TConsActor> cons_actor;
    };
}