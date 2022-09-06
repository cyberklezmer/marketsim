#ifndef GABI_CONFIG_HPP
#define GABI_CONFIG_HPP


#include "msstrategies/neuralnetstrategy.hpp"
#include "msstrategies/greedystrategy.hpp"
#include "msneural/models.hpp"
#include "msneural/nets/actorcritic.hpp"
#include "msneural/rewards.hpp"
#include "msneural/data.hpp"
#include "msneural/utils.hpp"

#include <torch/torch.h>

namespace marketsim {
    constexpr bool verbose = true;  // neural net/strategy outputs to cout


    /* neural net settings */
    // state and actions
    constexpr int hidden_size = 256;  // number of neurons in a layer
    constexpr int n_steps = 5;  // returns are 5 steps into the future
    constexpr int spread_lim = 5;  // actions are only alpha +- spread_lim, beta +- spread_lim
    
    constexpr int cons_mult = 100;  // in case of continuous actions, the predicted value is multiplied by this number

    constexpr bool stack = false;
    //constexpr bool stack = true;  // stack state to look into history
    constexpr int stack_size = 5;
    constexpr int stack_dim = 0;
    constexpr int state_size = 4;

    //constexpr int stack_dim = 1;
    //constexpr int state_size = 4 * stack_size;

    // strategy settings
    // sell limits - note that if both money and stocks are low, the strategy will try to trade some
    constexpr bool use_fixed_consumption = true;
    constexpr bool use_naive_cons = true;  // consumption like in naive mm

    constexpr int fixed_cons = 200;
    constexpr int with_stocks = true;

    constexpr int cons_lim = use_fixed_consumption ? 13000 : 500;  // don't consume if you have less than this

    //constexpr int volume = 10;  // volume for bids and asks
    constexpr int volume = 1;

    constexpr int cons_step = 125;  // for discrete actions, the max consumption is cons_parts * cons_step
    constexpr int cons_parts = 4;  // number of consumption steps
    constexpr bool entropy_reg = true;  // entropy regularization for more exploration

    // greedy strategy settings
    constexpr bool random_strategy = true;  // choose bid/ask values randomly
    
    //using state_layer = torch::nn::LSTM;
    using state_layer = torch::nn::Linear;

    using ba_discrete = DiscreteActions<hidden_size, 2 * spread_lim + 1, spread_lim>;
    using cons_discrete = DiscreteActions<hidden_size, cons_parts + 1, 0, cons_step>;
    using ba_cont = ContinuousActions<tanh_activation, softplus_activation, hidden_size, 1>;
    using cons_cont = ContinuousActions<softplus_activation, softplus_activation, hidden_size, 1, cons_mult>; 

    using dactor = BidAskActor<state_size, hidden_size, state_layer, ba_discrete, ba_discrete, cons_discrete, !use_fixed_consumption>;
    using cactor = BidAskActor<state_size, hidden_size, state_layer, ba_cont, ba_cont, cons_cont, !use_fixed_consumption>;
    using dflagactor = BidAskActorFlags<state_size, hidden_size, state_layer, ba_cont, ba_cont, cons_cont, !use_fixed_consumption>;

    using actor = dactor;
    using critic = Critic<state_size, hidden_size, state_layer>;
    using model = ActorCritic<actor, critic>;
    
    constexpr bool clear_batch = false;
    constexpr int batch_size = 32;
    constexpr int replay_buffer_max = 1000;
    using returns_func = DiffReturn;
    //using batcher = NextStateBatcher<n_steps, returns_func, stack, stack_size, stack_dim, batch_size, clear_batch>;
    //using batcher = NextStateBatcher<n_steps, returns_func, stack, stack_size, stack_dim>;  //batch_size == 1
    using batcher = ReplayBufferBatcher<n_steps, returns_func, batch_size, replay_buffer_max, 250, stack, stack_size, stack_dim>;

    // mm settings
    using order = neuralmmorder<volume, verbose>;
    using neuralstrategy = neuralnetstrategy<model, batcher, order, spread_lim, verbose, !use_fixed_consumption, fixed_cons, use_naive_cons>;
    using greedystrat = greedystrategy<cons_lim, verbose, random_strategy>;

    // speculator settings
    using spec_order = neuralspeculatororder<volume, verbose>;
    using spec_actor = BidAskActorSpeculator<state_size, hidden_size, state_layer, cons_discrete, !use_fixed_consumption>;
    using spec_model = ActorCritic<spec_actor, critic>;

    using spec_neuralstrategy = neuralnetstrategy<spec_model, batcher, order, spread_lim, verbose, !use_fixed_consumption, fixed_cons, use_naive_cons>;

}

#endif  //GABI_CONFIG_HPP_