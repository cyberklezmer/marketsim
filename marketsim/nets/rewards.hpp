#include <torch/torch.h>
#include <vector>
#include "proba.hpp"
#include "nets/utils.hpp"


namespace marketsim {
    double get_money(torch::Tensor state) {
        return state[0][0].item<double>();
    }

    double get_stocks(torch::Tensor state) {
        return state[0][1].item<double>();
    }

    double get_beta(torch::Tensor state) {
        return state[0][3].item<double>();
    }

    template <int N>
    class DiffReturn {
    public:
        DiffReturn() : curr_gamma(1.0), gamma(0.99998) {}
        virtual ~DiffReturn() {}
        
        torch::Tensor compute_returns(const std::vector<hist_entry>& history, torch::Tensor next_state) {
            curr_gamma = 1.0;
            double returns = 0.0;

            size_t hist_size = history.size();
            init(history, next_state);

            for (int i = hist_size - N; i < hist_size; ++i) {
                    auto entry = history.at(i);
                    torch::Tensor next = (i < hist_size - 1) ? history.at(i + 1).state : next_state;

                    double cons =  entry.actions.cons.item<double>();
                    double mdiff = get_money_diff(entry.state, next);
                    double sdiff = get_stock_diff(entry.state, next);

                    returns += compute_reward(cons, mdiff, sdiff);
                    curr_gamma *= gamma;
                }

            return torch::tensor({returns}).reshape({1,1});
        }

        double get_gamma() {
            return curr_gamma;
        }

        virtual void init(const std::vector<hist_entry>& history, torch::Tensor next_state) {}
        virtual double compute_reward(double cons, double mdiff, double sdiff) {
            return cons + curr_gamma * (mdiff + sdiff);
        }

    private:
        double get_money_diff(torch::Tensor state, torch::Tensor next_state) {
                return get_money(next_state) - get_money(state);        }

        double get_stock_diff(torch::Tensor state, torch::Tensor next_state) {
                double next_stock = get_beta(next_state) * get_stocks(next_state);
                double stock = get_stocks(state) * get_beta(state);
                
                return next_stock - stock;
        }

        double gamma, curr_gamma;
    };

    template <int N, int TMinM, int TMinS, bool verbose = false, bool separately = true>
    class WeightedDiffReturn : public DiffReturn<N> {
    public:
        WeightedDiffReturn() : DiffReturn<N>(), mdiff_mult(1.0), sdiff_mult(1.0) {}

        virtual void init(const std::vector<hist_entry>& history, torch::Tensor next_state) {
            size_t idx = history.size() - N;

            torch::Tensor state = history.at(idx).state;
            double state_money = get_money(state);
            double state_stocks = get_stocks(state);

            if (separately) {
                mdiff_mult = TMinM / (state_money + 100);
                sdiff_mult = TMinS / (state_stocks + 1);
            }
            else {
                double value = get_beta(state) * state_stocks + state_money;
                mdiff_mult = TMinM / value + 1;
                sdiff_mult = mdiff_mult;
            }

            //mdiff_mult = 20 * (-std::atan(state_money / 500) + pi() / 2);
            //sdiff_mult = 1 * (-std::atan(state_stocks - 10) + pi() / 2);
        }

        virtual double compute_reward(double cons, double mdiff, double sdiff) {
            if (verbose) {
                std::cout << "Returns - Cons: " << cons << ", Mdiff: " << mdiff_mult << ", Sdiff: " << sdiff_mult << std::endl;
            }

            return cons + this->get_gamma() * (mdiff_mult * mdiff + sdiff_mult * sdiff);
        }

        double mdiff_mult;
        double sdiff_mult;
    };
}