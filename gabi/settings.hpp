#ifndef GABI_CONFIG_HPP
#define GABI_CONFIG_HPP

#include "marketsim.hpp"
#include "msstrategies/neuralnetstrategy.hpp"
#include "msstrategies/greedystrategy.hpp"
#include "msneural/config.hpp"
#include "msneural/models.hpp"
#include "msneural/nets/actorcritic.hpp"
#include "msneural/rewards.hpp"
#include "msneural/data.hpp"
#include "msneural/utils.hpp"

#include <torch/torch.h>

namespace marketsim {

    using lstm = torch::nn::LSTM;
    using linear = torch::nn::Linear;

    static const char path[] = "../gabi/config.ini";
    using config = neural_config<path>; 

    // actions
    using ba_cont = ContinuousActions<tanh_activation, softplus_activation>;
    using cons_cont = ContinuousActions<softplus_activation, softplus_activation>;

    template <typename TLayer> using dactor = BidAskActor<config, TLayer, DiscreteActions, DiscreteActions>;
    template <typename TLayer> using cactor = BidAskActor<config, TLayer, ba_cont, cons_cont>;
    template <typename TLayer> using dflagactor = BidAskActorFlags<config, TLayer, DiscreteActions, DiscreteActions>;
    template <typename TLayer> using cflagactor = BidAskActorFlags<config, TLayer, ba_cont, cons_cont>;

    template <typename TLayer> using critic = Critic<config, TLayer>;
    
    template <typename TLayer> using dmodel = ActorCritic<config, dactor<TLayer>, critic<TLayer>>;
    template <typename TLayer> using cmodel = ActorCritic<config, cactor<TLayer>, critic<TLayer>>;
    template <typename TLayer> using dflagmodel = ActorCritic<config, dflagactor<TLayer>, critic<TLayer>>;
    template <typename TLayer> using cflagmodel = ActorCritic<config, cflagactor<TLayer>, critic<TLayer>>;
    
    using returns_func = DiffReturn;
    using batcher = NextStateBatcher<config, returns_func>;
    using buffer_batcher = ReplayBufferBatcher<config, returns_func>;

    // mm settings
    using order = neuralmmorder<config>;
    template <typename TLayer, typename TBatcher> using dneuralstrategy = neuralnetstrategy<config, dmodel<TLayer>, TBatcher, order>;
    template <typename TLayer, typename TBatcher> using cneuralstrategy = neuralnetstrategy<config, cmodel<TLayer>, TBatcher, order>;
    template <typename TLayer, typename TBatcher> using dflagneuralstrategy = neuralnetstrategy<config, dflagmodel<TLayer>, TBatcher, order>;
    template <typename TLayer, typename TBatcher> using cflagneuralstrategy = neuralnetstrategy<config, cflagmodel<TLayer>, TBatcher, order>;

    // speculator settings
    using spec_order = neuralspeculatororder<config>;
    template <typename TLayer> using spec_dactor = BidAskActorSpeculator<config, TLayer, DiscreteActions>;
    template <typename TLayer> using spec_cactor = BidAskActorSpeculator<config, TLayer, cons_cont>;
    template <typename TLayer> using spec_dmodel = ActorCritic<config, spec_dactor<TLayer>, critic<TLayer>>;
    template <typename TLayer> using spec_cmodel = ActorCritic<config, spec_cactor<TLayer>, critic<TLayer>>;

    template <typename TLayer, typename TBatcher> using spec_dneuralstrategy = neuralnetstrategy<config, spec_dmodel<TLayer>, TBatcher, order>;
    template <typename TLayer, typename TBatcher> using spec_cneuralstrategy = neuralnetstrategy<config, spec_dmodel<TLayer>, TBatcher, order>;

    template<typename TLayer> using qnet = QNet<config, critic<TLayer>>;
    template<typename TLayer, typename TBatcher> using qnet_strategy = neuralnetstrategy<config, qnet<TLayer>, TBatcher, order>;

    using greedystrat = greedystrategy<250, true, true>;

    /* CHOOSING THE STRATEGY */

    // these methods serve to decide if discrete actions should be used, flags, speculator...
    // They are used because templates have to be defined in compile-time, so this serves as a strategy factory
    template <typename TLayer, typename TBatcher, bool chronos>
    std::unique_ptr<competitorbase<chronos>> get_speculator(bool discrete, const std::string& aname) {
        if (discrete) {
            return std::make_unique<competitor<spec_dneuralstrategy<TLayer, TBatcher>, chronos>>(aname);
        }
        else {
            return std::make_unique<competitor<spec_cneuralstrategy<TLayer, TBatcher>, chronos>>(aname);
        }
    }

    template <typename TLayer, typename TBatcher, bool chronos>
    std::unique_ptr<competitorbase<chronos>> get_mm_flags(bool discrete, const std::string& aname) {
        if (discrete) {
            return std::make_unique<competitor<dflagneuralstrategy<TLayer, TBatcher>, chronos>>(aname);
        }
        else {
            return std::make_unique<competitor<cflagneuralstrategy<TLayer, TBatcher>, chronos>>(aname);
        }
    }

    template <typename TLayer, typename TBatcher, bool chronos>
    std::unique_ptr<competitorbase<chronos>> get_mm(bool discrete, const std::string& aname) {
        if (discrete) {
            return std::make_unique<competitor<dneuralstrategy<TLayer, TBatcher>, chronos>>(aname);
        }
        else {
            return std::make_unique<competitor<cneuralstrategy<TLayer, TBatcher>, chronos>>(aname);
        }
    }

    template <typename TLayer, typename TBatcher, bool chronos>
    std::unique_ptr<competitorbase<chronos>> get_mm_or_spec(const std::string& aname) {
        auto cfg = config::config;

        if (cfg->strategy.speculator) {
            return get_speculator<TLayer, TBatcher, chronos>(cfg->discrete, aname);
        }

        if (cfg->flags) {
            return get_mm_flags<TLayer, TBatcher, chronos>(cfg->discrete, aname);
        }

        if (cfg->model.qnet) {
            return std::make_unique<competitor<qnet_strategy<TLayer, TBatcher>, chronos>>(aname);
        }

        return get_mm<TLayer, TBatcher, chronos>(cfg->discrete, aname);
    }

    template <typename TLayer, bool chronos>
    std::unique_ptr<competitorbase<chronos>> get_batcher(const std::string& aname) {
        auto cfg = config::config;

        if (cfg->batcher.type == batcher_type::replay) {
            return get_mm_or_spec<TLayer, buffer_batcher, chronos>(aname);
        }
        else if (cfg->batcher.type == batcher_type::next_state) {
            return get_mm_or_spec<TLayer, batcher, chronos>(aname);
        }
        else {
            throw std::invalid_argument("Invalid batcher.");
        }
    }

    template <bool chronos>
    std::unique_ptr<competitorbase<chronos>> get_strategy(const std::string& aname) {
        auto cfg = config::config;

        if (cfg->layer.lstm) {
            return get_batcher<lstm, chronos>(aname);
        }
        else {
            return get_batcher<linear, chronos>(aname);
        }
    }
}

#endif  //GABI_CONFIG_HPP_