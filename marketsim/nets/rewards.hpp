#include <torch/torch.h>
#include <vector>


namespace marketsim {
    using hist_entry = std::tuple<torch::Tensor, torch::Tensor, torch::Tensor>;

    double get_money(const torch::Tensor& state) {
        return state[0][0].item<double>();
    }

    double get_stocks(const torch::Tensor& state) {
        return state[0][1].item<double>();
    }

    double get_beta(const torch::Tensor& state) {
        return state[0][3].item<double>();
    }

    template <int N>
    class DiffReturn {
    public:
        DiffReturn() : curr_gamma(1.0), gamma(0.99998) {}
        virtual ~DiffReturn() {}
        
        torch::Tensor compute_returns(const std::vector<hist_entry>& history, const torch::Tensor & next_state) {
            curr_gamma = 1.0;
            double returns = 0.0;

            size_t hist_size = history.size();
            init(history, next_state);

            for (int i = hist_size - N; i < hist_size; ++i) {
                    auto entry = history.at(i);
                    torch::Tensor next = (i < hist_size - 1) ? std::get<0>(history.at(i + 1)) : next_state;

                    double cons =  std::get<2>(entry).item<double>();
                    double mdiff = get_money_diff(std::get<0>(entry), next);
                    double sdiff = get_stock_diff(std::get<0>(entry), next);

                    returns += compute_reward(cons, mdiff, sdiff);
                    curr_gamma *= gamma;
                }

            return torch::tensor({returns}).reshape({1,1});
        }

        double get_gamma() {
            return curr_gamma;
        }

        virtual void init(const std::vector<hist_entry>& history, const torch::Tensor & next_state) {}
        virtual double compute_reward(double cons, double mdiff, double sdiff) {
            return cons + curr_gamma * (mdiff + sdiff);
        }

    private:
        double get_money_diff(const torch::Tensor& state, const torch::Tensor& next_state) {
                return get_money(next_state) - get_money(state);        }

        double get_stock_diff(const torch::Tensor& state, const torch::Tensor& next_state) {
                double next_stock = get_beta(next_state) * get_stocks(next_state);
                double stock = get_beta(state) * get_stocks(state);
                
                return next_stock - stock;
        }

        double gamma, curr_gamma;
    };

    template <int N, int TMinM, int TMinS>
    class WeightedDiffReturn : public DiffReturn<N> {
    public:
        WeightedDiffReturn() : DiffReturn<N>(), mdiff_mult(1.0), sdiff_mult(1.0) {}

        virtual void init(const std::vector<hist_entry>& history, const torch::Tensor & next_state) {
            size_t idx = history.size() - N;

            auto curr = history.at(idx);
            torch::Tensor state = std::get<0>(curr);
            double state_money = get_money(state);
            double state_stocks = get_stocks(state);

            mdiff_mult = TMinM / state_money;
            sdiff_mult = TMinS / state_stocks;
        }

        virtual double compute_reward(double cons, double mdiff, double sdiff) {
            return cons + mdiff_mult * mdiff + sdiff_mult * sdiff;
        }

        double mdiff_mult;
        double sdiff_mult;
    };
}