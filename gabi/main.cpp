#include <vector>
#include <iostream>
#include <limits>
#include <fstream>
#include <algorithm>
#include <random>

#include "marketsim.hpp"
#include "marketsim/competition.hpp"
#include "mscompetitions/dsmaslovcompetition.hpp"
#include "mscompetitions/originalcompetition.hpp"
#include "marketsim/tests.hpp"
#include "msstrategies/naivemmstrategy.hpp"
#include "msstrategies/liquiditytakers.hpp"
#include "msstrategies/firstsecondstrategy.hpp"
#include "msstrategies/neuralnetstrategy.hpp"
#include "msstrategies/greedystrategy.hpp"
#include "msstrategies/initialstrategy.hpp"
#include "msstrategies/maslovstrategy.hpp"

#include "nets/actorcritic.hpp"
#include "nets/trainer.hpp"
#include "nets/rewards.hpp"
#include "nets/utils.hpp"

#include <torch/torch.h>

using namespace marketsim;

int main()
{
    // Random state for networks
    torch::manual_seed(42);

    std::cout << "Neural net library test: " << std::endl;
    torch::Tensor tensor = torch::rand({2, 3});
    std::cout << tensor << std::endl;

    try
    {
        /* simulation settings */

        // change accordingly (but true is still beta),
        constexpr bool chronos = false;

        // change accordingly
        constexpr bool logging = true;

        // if false, run only the tested strategy (without the naive mm)
        bool with_mm = true;

        constexpr bool verbose = true;  // neural net/strategy outputs to cout

        // change accordingly (one unit rougly corresponds to one second)
        constexpr tabstime runningtime = 1000;
        //constexpr tabstime runningtime = 5000;
        //constexpr tabstime runningtime = 8 * 3600;

        // change accordingly
        twallet endowment(5000,100);

        tmarketdef def;

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
        constexpr int cons_lim = 500;  // don't consume if you have less than this
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

        using dnetwork = ActorCritic<state_size, hidden_size, state_layer, bid_ask_discrete, bid_ask_discrete, cons_discrete>;
        using cnetwork = ActorCritic<state_size, hidden_size, state_layer, bid_ask_cont, bid_ask_cont, cons_cont>;
        using network = cnetwork;  // change to dnetwork to use discrete actions

        constexpr int money_div = 1000;  // in the reward, weight money difference by money_div / money
        constexpr int stock_div = 10; // weight stock value difference by stock_div / stock
        constexpr bool separately = true;  // if true, the weights are computed using the overall value

        using wreturns_func = WeightedDiffReturn<n_steps, money_div, stock_div, verbose, separately>;
        using dreturns_func = DiffReturn<n_steps>;
        using returns_func = wreturns_func;  // change to dreturns_funct to use returns that are not weighted
        
        using trainer = NStepTrainer<network, n_steps, returns_func, entropy_reg, stack, stack_dim, stack_size>;

        // mm settings
        using order = neuralmmorder<keep_stocks, volume, verbose>;
        using neuralstrategy = neuralnetstrategy<trainer, order, cons_lim, spread_lim, cons_step, verbose>;
        using greedystrategy = greedystrategy<cons_lim, verbose, random_strategy>;

        // speculator settings
        using spec_order = neuralspeculatororder<volume, verbose>;
        //using spec_network = ACDiscrete<state_size, hidden_size, 2, cons_step * cons_parts, cons_parts, state_layer, true>;
        //using spec_trainer = NStepTrainer<spec_network, n_steps, returns_func, entropy_reg, stack, stack_dim, stack_size>;
        //using spec_neuralstrategy = neuralnetstrategy<spec_trainer, spec_order, cons_lim, spread_lim, cons_step, verbose, true, true, false>;

        //using testedstrategy = greedystrategy;
        using testedstrategy = neuralstrategy;  // change to greedy for greedy strategy competition
        //using testedstrategy = spec_neuralstrategy;  // speculator

        enum ewhattodo { esinglerunsinglestrategy,
                         erunall,
                         esinglerunstrategyandmaslovwithlogging,
                         emaslovcompetition,
                         eoriginalcompetition };


        // change accordingly
        ewhattodo whattodo = emaslovcompetition;

        /*  built in strategies*/

        // this strategy makes 40 random orders during the first second
        std::string fsname = "fs";
        competitor<firstsecondstrategy<40,10>,chronos,true> fss(fsname);

        // this strategy simulates liquidity takers:
        // 360 times per hour on average it puts a market order with volume 5 on average
        std::string ltname = "lt";
        competitor<liquiditytakers<360,5>,chronos,true> lts(ltname);


        /* competing strategies */

        // this strategy just wants to put orders into spread and when it
        // has more money than five times the price, it consumes. The volume of
        // the orders is always 10
        std::string nname = "naivka_10vol";
        competitor<naivemmstrategy<10>,chronos> nmm(nname);

        // our ingenious strategy
        std::string name = "neuronka";
        competitor<testedstrategy,chronos> ts(name);

        std::string gname = "greedy";
        competitor<greedystrategy,chronos> gs(name);

        std::string masl = "maslov";
        competitor<maslovstrategy,chronos> ms(masl);

        std::vector<competitorbase<chronos>*> competitors;

        switch(whattodo)
        {
        case esinglerunsinglestrategy:
            {
                competitor<testedstrategy,chronos> s;
                test<chronos,true,logging>({&s},runningtime,endowment,def);
            }
            break;
        case erunall:
            competitors.push_back(&fss);
            competitors.push_back(&lts);
            if (with_mm) {
                competitors.push_back(&nmm);
            }
            competitors.push_back(&ts);
            test<chronos,true,logging>(competitors, runningtime, endowment, def);
            break;
        case esinglerunstrategyandmaslovwithlogging:
            {
                competitors.push_back(&ts);
                competitors.push_back(&ms);

                // we will log the settlements
                def.loggingfilter.fsettle = true;
                // we wish to log also log entries by m, so we add its index
                // (see m's event handler to see how strategy can issue log entries)
                def.loggedstrategies.push_back(1);
                // the log is sent to std::cout
                auto data = test<chronos,true,true>(competitors,runningtime,endowment,def);
                const tmarkethistory& h=data->fhistory;
                tabstime t = 0;
                unsigned n = 30;
                tabstime dt = runningtime / n;

                std::cout << "t,b,a,p,definedp" << std::endl;
                for(unsigned i=0; i<=n; i++, t+=dt)
                {
                    auto s = h(t);
                    std::cout << t << "," << s.b << "," << s.a << "," << s.p() << ",";
                    if(i)
                        std::cout << h.definedpin(t-dt,t);
                    std::cout << std::endl;
                }
            }
            break;
        case emaslovcompetition:
        case eoriginalcompetition:
            {
                competitors.push_back(&ts);
                if (with_mm) {
                    competitors.push_back(&nmm);
                }

                def.loggingfilter.fsettle = true;
                def.loggedstrategies.push_back(0);

                tcompetitiondef cdef;
                cdef.timeofrun = runningtime;
                cdef.marketdef = def;

                if(whattodo==emaslovcompetition)
                    dsmaslovcompetition<chronos,logging>(competitors, endowment, cdef, std::clog);
                else
                    originalcompetition<chronos,logging>(competitors, endowment, cdef, std::clog);
            }
            break;
        default:
            throw "unknown option";
        }
        
        std::clog << "Done!" << std::endl;
        return 0;
    }
    catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    catch (const char* m)
    {
           std::cerr << m << std::endl;
           return 1;
    }
    catch (const std::string& m)
    {
           std::cerr << m << std::endl;
           return 1;
    }
    catch(...)
    {
        std::cerr << "unknown exception" << std::endl;
        return 1;
    }

    return 0;
}
