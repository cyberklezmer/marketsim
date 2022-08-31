#ifndef NET_DATA_HPP_
#define NET_DATA_HPP_

#include "nets/utils.hpp"
#include <torch/torch.h>
#include "marketsim.hpp"


namespace marketsim {
    //TODO stack with previous states

    template <int NSteps, typename TReturns>
    class BaseBatcher {
    public:
        BaseBatcher<NSteps>() : n_step_buffer(), returns_func() {}

        void add_next_state_action(torch::Tensor state, action_container<torch::Tensor> actions) {
            n_step_buffer.push_back(hist_entry(state, actions));

            if (n_step_buffer.size() <= NSteps) {
                return;
            }

            n_step_buffer.pop_front();
        }

        void update_returns(torch::Tensor next_state) {
            return update_returns(next_state, 0);
        }

        void update_returns(torch::Tensor next_state, double next_state_value) {
            if (n_step_buffer.size() < NSteps) {
                return;
            }

            double returns = returns_func.compute_returns(n_step_buffer, next_state);
            returns += next_state_value * returns_func.curr_gamma();
            
            hist_entry front = n_step_buffer.front();
            add_hist_entry(hist_entry(front.state, front.actions, torch::tensor({returns})));
        }

        virtual void add_hist_entry(hist_entry entry) = 0;
        virtual bool is_batch_ready() = 0;
        virtual hist_entry next_batch() = 0;

    private:
        std::deque<hist_entry> n_step_buffer;
        TReturns returns_func;
    };


    template <int NSteps, typename TReturns>
    class NextStateBatcher : BaseBatcher<NSteps, TReturns> {
    public:
        NextStateBatcher() : BaseBatcher<NSteps, TReturns>(), next_entry(hist_entry::empty_entry()), is_ready(false) {}

        void add_hist_entry(hist_entry entry) {
            is_ready = true;
            next_entry = entry;
        }

        bool is_batch_ready() {
            return is_ready;
        }

        hist_entry next_batch() {
            return hist_entry(next_entry.state.view({1, -1}), actions_view(next_entry.actions, {1, -1}),
                                next_entry.returns.view({1, -1}));
        }
        

    private:
        bool is_ready;
        hist_entry next_entry;
    };

    template <int NSteps, typename TReturns, int batch_size>
    class ReplayBufferBatcher : BaseBatcher<NSteps, TReturns> {
    public:
        ReplayBufferBatcher() : BaseBatcher<NSteps, TReturns>() {}

        virtual void add_hist_entry(hist_entry entry) {
            buffer.push_back(entry);
        }

        bool is_batch_ready() {
            return buffer.size() >= batch_size;
        }

        virtual hist_entry next_batch() {
            std::vector<int> batch_ids;
            for (size_t i = 0; i < buffer.size(); ++i) {
                batch_ids.push_back(random_int_from_tensor(0, buffer.size()));
            }

            std::vector<hist_entry> batch = map_func<hist_entry>(batch_ids, [&](int i){ return buffer[i]; });
            std::vector<torch::Tensor> b_states = map_func<torch::Tensor>(batch, [](hist_entry e){ return e.state; });
            std::vector<ac> b_actions = map_func<ac>(batch, [](hist_entry e){ return e.actions; });
            std::vector<torch::Tensor> b_ret = map_func<torch::Tensor>(batch, [](hist_entry e){ return e.returns; });
            
            return hist_entry(torch::cat(b_states, 0), actions_batch(b_actions), torch::cat(b_ret, 0));
        }
    
    private:
        using ac = action_container<torch::Tensor>;

        std::vector<hist_entry> buffer;
    };

}

#endif  //NET_DATA_HPP_