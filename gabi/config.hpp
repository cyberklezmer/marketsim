
#include "msstrategies/neuralnetstrategy.hpp"
#include "msstrategies/greedystrategy.hpp"
#include "nets/actorcritic.hpp"
#include "nets/trainer.hpp"
#include "nets/rewards.hpp"
#include "nets/utils.hpp"

#include <torch/torch.h>

namespace marketsim {
    constexpr bool verbose = true;  // neural net/strategy outputs to cout


    /* neural net settings */
    // state and actions
    constexpr int hidden_size = 256;  // number of neurons in a layer
    constexpr int n_steps = 5;  // returns are 5 steps into the future
    constexpr int spread_lim = 5;  // actions are only alpha +- spread_lim, beta +- spread_lim
    
    constexpr int cons_mult = 100;  // in case of continuous actions, the predicted value is multiplied by this number

    constexpr bool stack = false;  // stack state to look into history
    constexpr int stack_dim = 0;
    constexpr int stack_size = 5;
    constexpr int state_size = 4;

    //constexpr int stack_dim = 1;
    //constexpr int state_size = 4 * stack_size;

    // strategy settings
    // sell limits - note that if both money and stocks are low, the strategy will try to trade some
    constexpr bool use_fixed_consumption = true;
    constexpr bool use_naive_cons = true;  // consumption like in naive mm

    constexpr int fixed_cons = 200;
    constexpr int with_stocks = true;
    constexpr bool explore = !use_fixed_consumption;

    constexpr int cons_lim = use_fixed_consumption ? 13000 : 500;  // don't consume if you have less than this
    constexpr int keep_stocks = 10;  // don't sell if you have less than 10

    constexpr int volume = 10;  // volume for bids and asks

    constexpr int cons_step = 125;  // for discrete actions, the max consumption is cons_parts * cons_step
    constexpr int cons_parts = 4;  // number of consumption steps
    constexpr bool entropy_reg = true;  // entropy regularization for more exploration

    // greedy strategy settings
    constexpr bool random_strategy = true;  // choose bid/ask values randomly
    
    //using state_layer = torch::nn::LSTM;
    using state_layer = torch::nn::Linear;

    using bid_ask_discrete = DiscreteActions<hidden_size, 2 * spread_lim + 1, spread_lim>;
    using cons_discrete = DiscreteActions<hidden_size, cons_parts + 1, 0, cons_step>;
    using bid_ask_cont = ContinuousActions<tanh_activation, softplus_activation, hidden_size, 1>;
    using cons_cont = ContinuousActions<softplus_activation, softplus_activation, hidden_size, 1, cons_mult>; 

    using dnetwork = ActorCritic<state_size, hidden_size, state_layer, bid_ask_discrete, bid_ask_discrete, cons_discrete, !use_fixed_consumption>;
    using cnetwork = ActorCritic<state_size, hidden_size, state_layer, bid_ask_cont, bid_ask_cont, cons_cont, !use_fixed_consumption>;
    using dflag_network = ActorCriticFlags<state_size, hidden_size, state_layer, bid_ask_discrete, bid_ask_discrete, cons_discrete, !use_fixed_consumption>;
    using network = dnetwork;  // change to dnetwork to use discrete actions

    constexpr int money_div = 1000;  // in the reward, weight money difference by money_div / money
    constexpr int stock_div = 10; // weight stock value difference by stock_div / stock
    constexpr bool separately = true;  // if true, the weights are computed using the overall value

    using wreturns_func = WeightedDiffReturn<n_steps, money_div, stock_div, verbose, separately>;
    using dreturns_func = DiffReturn<n_steps>;
    using returns_func = dreturns_func;  // change to dreturns_funct to use returns that are not weighted
    
    using trainer = NStepTrainer<network, n_steps, returns_func, entropy_reg, stack, stack_dim, stack_size>;

    // mm settings
    using order = neuralmmorder<keep_stocks, volume, verbose>;
    using neuralstrategy = neuralnetstrategy<trainer, order, cons_lim, spread_lim, cons_step, verbose, true, explore, true, !use_fixed_consumption, fixed_cons, with_stocks, use_naive_cons>;
    using greedystrat = greedystrategy<cons_lim, verbose, random_strategy>;

    // speculator settings
    using spec_order = neuralspeculatororder<volume, verbose>;
    using spec_network = ActorCriticSpeculator<state_size, hidden_size, state_layer, cons_discrete, !use_fixed_consumption>;
    using spec_trainer = NStepTrainer<spec_network, n_steps, returns_func, entropy_reg, stack, stack_dim, stack_size>;
    using spec_neuralstrategy = neuralnetstrategy<spec_trainer, spec_order, cons_lim, spread_lim, cons_step, verbose, true, explore, false, !use_fixed_consumption, fixed_cons, with_stocks, use_naive_cons>;

}