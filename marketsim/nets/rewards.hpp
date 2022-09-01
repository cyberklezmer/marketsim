#include <torch/torch.h>
#include <vector>
#include "proba.hpp"
#include "nets/utils.hpp"


namespace marketsim {
    double get_money(torch::Tensor state) {
        return state[0].item<double>();
    }

    double get_stocks(torch::Tensor state) {
        return state[1].item<double>();
    }

    double get_beta(torch::Tensor state) {
        return state[3].item<double>();
    }

    template <int N>
    class DiffReturn {
    public:
        DiffReturn() : curr_gamma(1.0), gamma(0.99998) {}
        virtual ~DiffReturn() {}
        
        template <typename T>
        torch::Tensor compute_returns(const T& history, torch::Tensor next_state) {
            curr_gamma = 1.0;
            double returns = 0.0;
            
            const hist_entry* prev = nullptr;
            for (const hist_entry& entry : history) {
                if (prev == nullptr) {
                    prev = &entry;
                    continue;
                }

                returns += compute_reward(*prev, entry.state);
                curr_gamma *= gamma;
                prev = &entry;
            }
            returns += compute_reward(*prev, next_state);
            curr_gamma *= gamma;

            return torch::tensor({returns}).reshape({1,1});
        }

        double get_gamma() {
            return curr_gamma;
        }

        double compute_reward(hist_entry prev, torch::Tensor state) {
            double cons =  prev.actions.cons.item<double>();
            double mdiff = get_money_diff(prev.state, state);
            double sdiff = get_stock_diff(prev.state, state);

            return cons + curr_gamma * (mdiff + sdiff);
        }

    private:
        double get_money_diff(torch::Tensor state, torch::Tensor next_state) {
                return get_money(next_state) - get_money(state);
        }

        double get_stock_diff(torch::Tensor state, torch::Tensor next_state) {
                double next_stock = get_beta(next_state) * get_stocks(next_state);
                double stock = get_stocks(state) * get_beta(state);
                
                return next_stock - stock;
        }

        double gamma, curr_gamma;
    };

}