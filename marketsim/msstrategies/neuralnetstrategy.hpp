#include "marketsim.hpp"
#include <boost/math/constants/constants.hpp>
#include "nets/actorcritic.hpp"

namespace marketsim {

    torch::Tensor action_log_prob(torch::Tensor mu, torch::Tensor std, torch::Tensor x) {
        auto variance = std.pow(2);
        auto log_scale = std.log();
        auto subs = (-(x - mu).pow(2) / (2 * variance));
        return subs - log_scale - std::log(std::sqrt(2 * boost::math::constants::pi<double>()));
    }

    class neuralnetstrategy : public teventdrivenstrategy {
    public:
        neuralnetstrategy() : teventdrivenstrategy(1),
            net(std::make_unique<ActorCriticNet>()),
            optimizer(std::make_unique<torch::optim::SGD>(net->parameters(), /*lr=*/0.01)),
            history() {}

    private:
        virtual trequest event(const tmarketinfo& mi, tabstime t, trequestresult* lastresult) {
            if (history.size() > 0) {
                
            }

            // TODO ulož si prev state, action, spočítej reward (to je něco jako diff)
            // one step boostrap to je jako network pred value next state

            
            // TODO
            //   - state dostanu jako current state (proste veci z marketu atd)
            //   - action si napredikuju
            //   - a v tom ifu nahore si vemu posledni stav a akci, napredikuju value next_state a uz mam co potrebuju
            //      - akorat reward je asi diff, takze neco z minulyho si asi taky ulozim

            //history.push_back(std::make_pair(state, action));

            return trequest();
        }

        void train(torch::Tensor state, torch::Tensor actions, torch::Tensor returns) {
            

            auto pred = net->forward(state);
            auto action_mus = std::get<0>(pred);
            auto action_stds = std::get<1>(pred);
            auto state_value = std::get<2>(pred);

            auto advantages = (returns - state_value);
            auto value_loss = advantages.pow(2).mean(); //TODO minus dole správně?
            auto action_loss = -(advantages.detach() * action_log_prob(action_mus, action_stds, actions).sum()).mean();
            
            auto loss = value_loss + action_loss; //TODO entropy regularization

            optimizer->zero_grad();
            loss.backward();
            optimizer->step();
        }

        std::vector<std::pair<torch::Tensor, torch::Tensor>> history;
        std::unique_ptr<ActorCriticNet> net;
        std::unique_ptr<torch::optim::SGD> optimizer;
    };
}