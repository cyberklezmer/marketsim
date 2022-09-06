#ifndef NEURALNETSTRATEGY_HPP_
#define NEURALNETSTRATEGY_HPP_

#include "msneural/utils.hpp"
#include "marketsim.hpp"
#include <torch/torch.h>


namespace marketsim {

    template<typename TConfig, typename TNet, typename TBatcher, typename TOrder>
    class neuralnetstrategy : public teventdrivenstrategy {
    public:
        neuralnetstrategy() : teventdrivenstrategy(1),
            net(),
            order(),
            batcher(),
            history(),
            last_bid(klundefprice),
            last_ask(khundefprice) {
                auto cfg = TConfig::config;
                verbose = cfg->strategy.verbose;
                spread_lim = cfg->strategy.spread_lim;
                train_cons = cfg->strategy.train_cons;
                use_naive_cons = cfg->strategy.use_naive_cons;
                fixed_cons = cfg->strategy.fixed_cons;

                if (verbose) {
                    std::cout << std::endl << "--------------------" << std::endl << "Start of competition..." << std::endl;
                }
            }

    private:
        virtual trequest event(const tmarketinfo& mi, tabstime t, trequestresult* lastresult) {
            torch::Tensor next_state = this->construct_state(mi);
            torch::Tensor pred_state = batcher.transform_for_prediction(next_state);

            torch::Tensor state_value = net.predict_values(pred_state);
            batcher.update_returns(next_state, state_value);

            if (batcher.is_batch_ready()){
                hist_entry batch = batcher.next_batch();
                net.train(batch);
            }

            auto pred_actions = net.predict_actions(pred_state);
            batcher.add_next_state_action(next_state, pred_actions);

            set_consumption(mi, pred_actions);
            set_flags(pred_actions);

            if (spread_lim > 0) {
                limit_spread(pred_actions);
            }

            history.push_back(hist_entry(next_state, pred_actions));
            return order.construct_order(mi, pred_actions);
        }
        
        torch::Tensor construct_state(const tmarketinfo& mi) {
            tprice m = mi.mywallet().money();
            tprice s = mi.mywallet().stocks();

            auto ab = get_alpha_beta(mi, last_bid, last_ask);
            tprice a = std::get<0>(ab);
            tprice b = std::get<1>(ab);
            
            std::vector<float> state_data = std::vector<float>{float(m), float(s), float(a), float(b)};
            return torch::tensor(state_data);
        }

        void limit_spread(action_container<torch::Tensor>& next_action) {
            next_action.bid = torch::clamp(next_action.bid, -spread_lim, spread_lim);
            next_action.ask = torch::clamp(next_action.ask, -spread_lim, spread_lim);            
        }

        void set_consumption(const tmarketinfo& mi, action_container<torch::Tensor>& next_action) {
            if (train_cons) {
                return;
            }

            double next_cons = fixed_cons;
            if (use_naive_cons) {
                tprice m = mi.mywallet().money();
                tprice p = get_p(mi);
                next_cons = (m > 5 * p) ? (m - 5 * p) : 0;
            }

            next_action.cons = torch::tensor({next_cons});
        }

        void set_flags(action_container<torch::Tensor>& next_action) {
            if (!next_action.is_flag_valid()) {
                return;
            }

            double bid = next_action.bid_flag.item<double>();
            double ask = next_action.ask_flag.item<double>();
            double threshold = 0.5;

            bid = (bid > threshold) ? 1.0 : 0.0;
            ask = (ask > threshold) ? 1.0 : 0.0;
            
            next_action.bid_flag = torch::tensor({bid});
            next_action.ask_flag = torch::tensor({ask});
        }

        int spread_lim, fixed_cons;
        bool verbose, train_cons, use_naive_cons;

		tprice last_bid, last_ask;

        TNet net;
        TBatcher batcher;
        TOrder order;
        std::vector<hist_entry> history;
    };

    template <typename TConfig>
    class neuralspeculatororder {
    public:
        neuralspeculatororder() : threshold(0.5) {
            auto cfg = TConfig::config;
            verbose = cfg.strategy.verbose;
            volume = cfg.strategy.volume;
        } 

        trequest construct_order(const tmarketinfo& mi, const action_container<torch::Tensor>& actions) {
            double bpred = actions.bid_flag.item<double>();
            double apred = actions.ask_flag.item<double>();
            int cons = int(actions.cons.item<double>());

            if (verbose) {
                print_state(mi, bpred, apred, cons);
            }

            trequest o;
            if (bpred > threshold) {
                o.addbuymarket(volume);
            }

            if (apred > threshold) {
                o.addsellmarket(volume);
            }

            o.setconsumption(cons);
            return o;
        }
    
    private:
        bool verbose;
        int volume;

        double threshold;
    };

    template<typename TConfig>
    class neuralmmorder {
    public:
        neuralmmorder() : last_bid(klundefprice), last_ask(khundefprice), threshold(0.5) {
            auto cfg = TConfig::config;
            verbose = cfg->strategy.verbose;
            volume = cfg->strategy.volume;
        }

        trequest construct_order(const tmarketinfo& mi, const action_container<torch::Tensor>& actions) {
            double bpred = actions.bid.item<double>();
            double apred = actions.ask.item<double>();
            int cons = int(actions.cons.item<double>());

            auto ab = get_alpha_beta(mi, last_bid, last_ask);
            tprice a = std::get<0>(ab);
            tprice b = std::get<1>(ab);
            if (verbose) {
                std::cout << "Beta: " << b << ", Alpha: " << a << std::endl;
            }

            tprice bid = int(b + bpred);
            tprice ask = int(a + apred);
            
            bid = (bid < 0) ? 0 : bid;
            ask = (ask <= 0) ? 1 : ask;

            if (bid >= ask) {
                ask = bid + 1;
            }

            if (actions.is_flag_valid()) {
                double bid_flag = (actions.is_flag_valid()) ? actions.bid_flag.item<double>() : 1.0;
                double ask_flag = (actions.is_flag_valid()) ? actions.ask_flag.item<double>() : 1.0;

                if (bid_flag <= threshold) {
                    bid = klundefprice;
                }
                if (ask_flag <= threshold) {
                    ask = khundefprice;
                }
            }

            auto ot = create_order(bid, ask, cons);
            if (verbose) {
                print_state(mi, bid, ask, cons, bpred, apred);
            }

           if (bid_defined(bid)) last_bid = bid;
           if (ask_defined(ask)) last_ask = ask;
           
           return ot;
        }

    private:
        bool verbose;
        int volume;

        tprice last_bid, last_ask;
        double threshold;
    };
}

#endif // NEURALNETSTRATEGY_HPP_