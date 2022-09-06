#include <torch/torch.h>
#include "nets/proba.hpp"
#include "nets/actions.hpp"
using namespace torch::indexing;  


namespace marketsim {
    //TODO does this work when batched?

    torch::Tensor state_forward(torch::nn::LSTM layer, torch::Tensor x) {
        if (x.dim() == 2) {
            x = x.unsqueeze(1);
        }

        auto res = layer->forward(x);
        torch::Tensor out = std::get<0>(res);
        
        // get the last output
        out = out.index({Slice(), out.sizes()[1] - 1});
        
        return out;
    }

    torch::Tensor state_forward(torch::nn::Linear layer, torch::Tensor x) {
        return layer->forward(x);
    } 


    template <int state_size, int hidden_size, typename TLayer>
    class Critic : public torch::nn::Module {
    public:
        Critic() {
            state_layer = register_module("state_layer", TLayer(state_size, hidden_size));
            out = register_module("out", torch::nn::Linear(hidden_size, 1));
        }

        torch::Tensor forward(torch::Tensor x) {
            x = state_forward(this->state_layer, x);
            x = torch::relu(x);
            return out->forward(x);
        }

    private:
        TLayer state_layer{nullptr};    
        torch::nn::Linear out{nullptr};
    };


    template <int state_size, int hidden_size, typename TLayer, typename TBidActor, typename TAskActor, typename TConsActor, bool cons_on = true>
    class BidAskActor : public torch::nn::Module {
    public:
        BidAskActor() : zero_tensor(torch::zeros({1})) {
            state_layer = register_module("state_layer", TLayer(state_size, hidden_size));

            bid_actor = this->register_module("bid_actor", std::make_shared<TBidActor>());
            ask_actor = this->register_module("ask_actor", std::make_shared<TAskActor>());
            if (cons_on) {
                cons_actor = this->register_module("cons_actor", std::make_shared<TConsActor>());
            }
        }
        virtual ~BidAskActor() {}

        virtual action_container<torch::Tensor> predict_actions(torch::Tensor x) {
            auto res = predict_actions_train(x);
            return action_container<torch::Tensor>(
                bid_actor->sample_actions(res.bid),
                ask_actor->sample_actions(res.ask),
                cons_on ? cons_actor->sample_actions(res.cons) : zero_tensor
            );
        }

        virtual action_container<action_tensors> predict_actions_train(torch::Tensor x) {
            x = state_forward(this->state_layer, x);
            x = torch::relu(x);

            return action_container<action_tensors>(
                bid_actor->predict_actions(x),
                ask_actor->predict_actions(x),
                cons_on ? cons_actor->predict_actions(x) : action_tensors()
            );
        }

        virtual torch::Tensor action_log_prob(const action_container<torch::Tensor>& actions,
                                              const action_container<action_tensors>& pred) {
            torch::Tensor res = bid_actor->action_log_prob(actions.bid, pred.bid);
            res += ask_actor->action_log_prob(actions.ask, pred.ask);
            
            if (cons_on) {
                res += cons_actor->action_log_prob(actions.cons, pred.cons);
            }

            return res;
        }

        virtual torch::Tensor entropy(const action_container<action_tensors>& pred) {
            torch::Tensor res = bid_actor->entropy(pred.bid);
            res += ask_actor->entropy(pred.ask);

            if (cons_on) {
                res += cons_actor->entropy(pred.cons);
            }

            return res;
        }

    protected:
        torch::Tensor zero_tensor;

        TLayer state_layer{nullptr};

        std::shared_ptr<TBidActor> bid_actor{nullptr};
        std::shared_ptr<TAskActor> ask_actor{nullptr};
        std::shared_ptr<TConsActor> cons_actor{nullptr};
    };

    template <int state_size, int hidden_size, typename TLayer, typename TBidActor, typename TAskActor, typename TConsActor, bool cons_on = true>
    class BidAskActorFlags : public BidAskActor<state_size, hidden_size, TLayer, TBidActor, TAskActor, TConsActor, cons_on> {
    public:
        using Base = BidAskActor<state_size, hidden_size, TLayer, TBidActor, TAskActor, TConsActor, cons_on>;

        BidAskActorFlags() : Base() {
            bid_flag = this->register_module("bid_flag", std::make_shared<BinaryAction<hidden_size>>());
            ask_flag = this->register_module("ask_flag", std::make_shared<BinaryAction<hidden_size>>());
        }

        action_container<torch::Tensor> predict_actions(torch::Tensor x) {
            auto pred = predict_actions_train(x);
            
            return action_container<torch::Tensor>(
                this->bid_actor->sample_actions(pred.bid),
                this->ask_actor->sample_actions(pred.ask), 
                cons_on ? this->cons_actor->sample_actions(pred.cons) : this->zero_tensor,
                this->bid_flag->sample_actions(pred.bid_flag),
                this->ask_flag->sample_actions(pred.ask_flag)
            );
        }

        action_container<action_tensors> predict_actions_train(torch::Tensor x) {
            x = state_forward(this->state_layer, x);
            x = torch::relu(x);

            return action_container<action_tensors>(
                this->bid_actor->predict_actions(x),
                this->ask_actor->predict_actions(x),
                cons_on ? this->cons_actor->predict_actions(x) : action_tensors(),
                this->bid_flag->predict_actions(x),
                this->ask_flag->predict_actions(x)
            );
        }

        torch::Tensor action_log_prob(const action_container<torch::Tensor>& actions,
                                              const action_container<action_tensors>& pred) {
            torch::Tensor log_prob = Base::action_log_prob(actions, pred);
            log_prob += bid_flag->action_log_prob(actions.bid_flag, pred.bid_flag);
            log_prob += ask_flag->action_log_prob(actions.ask_flag, pred.ask_flag);
            return log_prob;
        }

        torch::Tensor entropy(const action_container<action_tensors>& pred) {
            torch::Tensor res = Base::entropy(pred);
            res += bid_flag->entropy(pred.bid_flag);
            res += ask_flag->entropy(pred.ask_flag);
            return res;
        }

    private:
        std::shared_ptr<BinaryAction<hidden_size>> bid_flag, ask_flag;
    };

    template <int state_size, int hidden_size, typename TLayer, typename TConsActor, bool cons_on = true>
    class BidAskActorSpeculator : public torch::nn::Module {
    public:
        BidAskActorSpeculator() : zero_tensor(torch::zeros({1})) {
            state_layer = register_module("state_layer", TLayer(state_size, hidden_size));

            bid_ask_actor = this->register_module("bid_ask_actor", std::make_shared<DiscreteActions<hidden_size, 3>>());
            cons_actor = this->register_module("cons_actor", std::make_shared<TConsActor>());
        }
            
         action_container<torch::Tensor> predict_actions(torch::Tensor x) {
            auto pred = predict_actions_train(x);
            torch::Tensor bid_ask = bid_ask_actor->sample_actions(pred.bid);

            double bid = (bid_ask.item<int>() == 1) ? 1.0 : 0.0;
            double ask = (bid_ask.item<int>() == 2) ? 1.0 : 0.0;
            
            return action_container<torch::Tensor>(
                this->zero_tensor,
                this->zero_tensor,
                cons_on ? this->cons_actor->sample_actions(pred.cons) : zero_tensor,
                torch::tensor({bid}),
                torch::tensor({ask})
            );
        }

        action_container<action_tensors> predict_actions_train(torch::Tensor x) {
            x = state_forward(this->state_layer, x);
            x = torch::relu(x);

            return action_container<action_tensors>(
                this->bid_ask_actor->predict_actions(x),
                action_tensors(),
                cons_on ? this->cons_actor->predict_actions(x) : action_tensors()
            );
        }

        torch::Tensor action_log_prob(const action_container<torch::Tensor>& actions,
                                      const action_container<action_tensors>& pred) {
            torch::Tensor true_target = construct_target(actions);
            torch::Tensor log_prob =  bid_ask_actor->action_log_prob(true_target, pred.bid);

            if (cons_on) {
                log_prob += cons_actor->action_log_prob(actions.cons, pred.cons);
            }

            return log_prob;
        }

        torch::Tensor entropy(const action_container<action_tensors>& pred) {
            torch::Tensor res = bid_ask_actor->entropy(pred.bid);
            if (cons_on) {
                res += cons_actor->entropy(pred.cons);
            }

            return res;
        }
        

    private:
        torch::Tensor construct_target(const action_container<torch::Tensor>& actions) {
            torch::Tensor bid = actions.bid_flag;
            torch::Tensor ask = actions.ask_flag;

            assert(((bid + ask) < 1.0001).all().item<bool>());
            
            return bid + 2 * ask;
        }

        torch::Tensor zero_tensor;
        TLayer state_layer{nullptr};

        std::shared_ptr<DiscreteActions<hidden_size, 3>> bid_ask_actor;
        std::shared_ptr<TConsActor> cons_actor;
    };
}
