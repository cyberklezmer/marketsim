#include <vector>
#include <iostream>
#include <limits>
#include <fstream>
#include <algorithm>
#include <random>

#include "marketsim.hpp"
#include "marketsim/competition.hpp"
#include "marketsim/tests.hpp"
#include "msstrategies/naivemmstrategy.hpp"
#include "msstrategies/liquiditytakers.hpp"
#include "msstrategies/firstsecondstrategy.hpp"
#include "msstrategies/neuralnetstrategy.hpp"
#include "msstrategies/greedystrategy.hpp"

#include "nets/actorcritic.hpp"
#include "nets/trainer.hpp"
#include "nets/rewards.hpp"

#include <torch/torch.h>

using namespace marketsim;

int main()
{
    torch::manual_seed(42);

    torch::Tensor tensor = torch::rand({2, 3});
    std::cout << tensor << std::endl;

    try
    {
        // change accordingly (but true is still beta),
        constexpr bool chronos = false;

        // change accordingly
        constexpr bool logging = false;

        // change accordingly (one unit rougly corresponds to one second)
        constexpr tabstime runningtime = 1000;
        //constexpr tabstime runningtime = 8 * 3600;

        // change accordingly
        twallet endowment(5000,100);

        tmarketdef def;

        // you can modify def, perhaps adjust logging etc

        // change accordingly but note that with chronos==true the streategies must be
        // descentants of marketsim::teventdrivenstrategy
        //using testedstrategy = maslovstrategy;

        bool with_mm = true;

        constexpr int n_steps = 2;
        constexpr int cons_mult = 10000;
        constexpr int cons_lim = 1000;
        constexpr int keep_stocks = 10;
        constexpr int spread_lim = 5;

        constexpr int volume = 10;
        constexpr bool modify_cons = true;

        constexpr int max_consumption = 10000;
        constexpr int cons_parts = 8;
        constexpr bool entropy_reg = true;
        
        using network = ACDiscrete<4, 256, 5, max_consumption, cons_parts>;
        //using network = ACContinuous<4, 256, 1, cons_mult>;

        constexpr int money_div = 1000;
        constexpr int stock_div = 100; 

        using returns_func = WeightedDiffReturn<n_steps, money_div, stock_div>;
        //using returns_func = DiffReturn<n_steps>;
        
        using trainer = NStepTrainer<network, n_steps, returns_func, entropy_reg>;

        using testedstrategy = neuralnetstrategy<trainer, cons_lim, keep_stocks, spread_lim, modify_cons, volume>;
          // should be the AI strategy
        //using testedstrategy = greedystrategy<>;

        enum ewhattodo { esinglerunsinglestrategy,
                         erunall,
                         ecompetition };



        // built in strategies

        // this strategy makes 40 random orders during the first second
        competitor<firstsecondstrategy<40,10>,chronos,true> fss;

        // this strategy simulates liquidity takers:
        // 360 times per hour on average it puts a market order with volume 5 on average
        competitor<liquiditytakers<360,5>,chronos,true> lts;


        // competing strategies


        // this strategy just wants to put orders into spread and when it
        // has more money than five times the price, it consumes. The volume of
        // the orders is always 10

        competitor<naivemmstrategy<10>,chronos> nmm;

        // our ingenious strategy
        competitor<testedstrategy,chronos> ts;

        // change accordingly
        ewhattodo whattodo = ecompetition;

        switch(whattodo)
        {
        case esinglerunsinglestrategy:
            {
                competitor<testedstrategy,chronos> s;
                test<chronos,true,logging>({&s},runningtime,endowment,def);
            }
            break;
        case erunall:
            if (with_mm) {
                test<chronos,true,logging>({&fss,&lts,&nmm,&ts}, runningtime, endowment, def);
            }
            else {
                test<chronos,true,logging>({&fss,&lts,&ts}, runningtime, endowment, def);
            }
            break;
        case ecompetition:
            if (with_mm) {
                competition<chronos,true,logging>({&fss,&lts,&nmm,&ts}, runningtime, endowment, def, std::clog);
                break;
            }
            else {
                competition<chronos,true,logging>({&fss,&lts,&ts}, runningtime, endowment, def, std::clog);
                break;
            }

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



