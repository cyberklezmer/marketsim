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

        // change accordingly (one unit rougly corresponds to one second)
        constexpr tabstime runningtime = 1000;
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

        // strategy settings
        // sell limits - note that if both money and stocks are low, the strategy will try to trade some
        constexpr int cons_lim = 500;  // don't consume if you have less than this
        constexpr int keep_stocks = 10;  // don't sell if you have less than 10

        constexpr int volume = 10;  // volume for bids and asks

        constexpr int cons_step = 125;  // for discrete actions, the max consumption is cons_parts * cons_step
        constexpr int cons_parts = 4;  // number of consumption steps
        constexpr bool entropy_reg = true;  // entropy regularization for more exploration
        
        using dnetwork = ACDiscrete<4, hidden_size, spread_lim, cons_step * cons_parts, cons_parts>;
        using cnetwork = ACContinuous<4, hidden_size, 1, cons_mult>;
        using network = cnetwork;  // change to dnetwork to use discrete actions

        constexpr int money_div = 1000;  // in the reward, weight money difference by money_div / money
        constexpr int stock_div = 10; // weight stock value difference by stock_div / stock

        using wreturns_func = WeightedDiffReturn<n_steps, money_div, stock_div>;
        using dreturns_func = DiffReturn<n_steps>;
        using returns_func = wreturns_func;  // change to dreturns_funct to use returns that are not weighted
        
        using trainer = NStepTrainer<network, n_steps, returns_func, entropy_reg>;

        using neuralstrategy = neuralnetstrategy<trainer, cons_lim, keep_stocks, spread_lim, cons_step, volume>;
        using greedystrategy = greedystrategy<>;

        using testedstrategy = neuralstrategy;  // change to greedy for greedy strategy competition

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
