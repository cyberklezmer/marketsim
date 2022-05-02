#include "marketsim.hpp"
#include "nets/actorcritic.hpp"

namespace marketsim {
    class neuralnetstrategy : public teventdrivenstrategy {
    public:
        neuralnetstrategy() : teventdrivenstrategy(1),
            net(std::make_unique<ActorCriticNet>()),
            optimizer(std::make_unique<torch::optim::SGD>(net->parameters(), /*lr=*/0.01)) {}

    private:
        virtual trequest event(const tmarketinfo& mi, tabstime t, trequestresult* lastresult) {
            torch::Tensor some_state = torch::rand({32, 20});

            auto pred = net->forward(some_state);
            auto action_log_probs = std::get<0>(pred);
            auto state_value = std::get<1>(pred);

            auto advantages = (torch::rand({32, 1}) - state_value);
            auto value_loss = advantages.pow(2).mean();
            auto action_loss = -(advantages.detach() * action_log_probs).mean();
            
            auto loss = value_loss + action_loss; //TODO entropy

            optimizer->zero_grad();
            loss.backward();
            optimizer->step();

            return trequest();
        }

        std::unique_ptr<ActorCriticNet> net;
        std::unique_ptr<torch::optim::SGD> optimizer;
    };
}