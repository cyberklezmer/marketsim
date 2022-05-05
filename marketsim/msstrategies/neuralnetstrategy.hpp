#include "marketsim.hpp"
#include "nets/actorcritic.hpp"

namespace marketsim {

    template<typename TNet>
    class neuralnetstrategy : public teventdrivenstrategy {
    public:
        neuralnetstrategy() : teventdrivenstrategy(1),
            net(std::make_unique<TNet>()),
            optimizer(std::make_unique<torch::optim::SGD>(net->parameters(), /*lr=*/0.01)),
            history(),
            gamma(0.99998) {}

    private:
        virtual trequest event(const tmarketinfo& mi, tabstime t, trequestresult* lastresult) {
            auto next_state = nullptr;
            auto reward = nullptr;
            
            if (history.size() > 0) {
                auto state = nullptr;
                auto action = nullptr;
                auto returns = reward + net->predict_values(next_state).detach();

                train(state, action, returns);
            }

            //TODO fce sample action do sítě
            
            // TODO
            //   - state dostanu jako current state (proste veci z marketu atd)
            //   - action si napredikuju
            //   - akorat reward je diff, takze neco z minulyho si asi taky ulozim

            //history.push_back(std::make_pair(state, action));

            return trequest();
        }

        void train(torch::Tensor state, torch::Tensor actions, torch::Tensor returns) {
            optimizer->zero_grad();

            auto pred = net->forward(state);
            auto pred_actions = std::get<0>(pred);
            auto state_value = std::get<1>(pred);

            auto advantages = (returns - state_value);
            auto value_loss = advantages.pow(2).mean(); 
            auto action_probs = net->action_log_prob(actions, pred_actions);
            auto action_loss = -(advantages.detach() * action_probs).mean(); //TODO minus správně?
            
            auto loss = value_loss + action_loss; //TODO entropy regularization

            loss.backward();
            optimizer->step();
        }

        double gamma;
        std::vector<std::pair<torch::Tensor, torch::Tensor>> history;
        std::unique_ptr<TNet> net;
        std::unique_ptr<torch::optim::SGD> optimizer;
    };
}