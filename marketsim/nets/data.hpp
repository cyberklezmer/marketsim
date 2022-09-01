#ifndef NET_DATA_HPP_
#define NET_DATA_HPP_

#include "nets/utils.hpp"
#include <torch/torch.h>
#include "marketsim.hpp"


namespace marketsim {
    template <int NSteps, typename TReturns, bool stack_history = false, int stack_size = 5, int stack_dim = 0>
    class BaseBatcher {
    public:
        BaseBatcher() : n_step_buffer(), prev_states(), prev_states_pred(), returns_func() {}

        void add_next_state_action(torch::Tensor state, action_container<torch::Tensor> actions) {
            n_step_buffer.push_back(hist_entry(state, actions));

            if (stack_history) {
                update_prev_states(prev_states, n_step_buffer.front().state);
            }

            if (n_step_buffer.size() <= NSteps) {
                return;
            }

            n_step_buffer.pop_front();
        }

        void update_returns(torch::Tensor next_state) {
            return update_returns(next_state, 0);
        }

        void update_returns(torch::Tensor next_state, torch::Tensor next_state_value) {
            if (n_step_buffer.size() < NSteps) {
                return;
            }

            torch::Tensor returns = returns_func.compute_returns(n_step_buffer, next_state);
            returns += next_state_value * returns_func.get_gamma();
            
            hist_entry front = n_step_buffer.front();

            // optionally stack previous states to keep info about past wallet and prices
            torch::Tensor state = stack_history ? stack_prev_states(prev_states, front.state) : transform_state(front.state);
            add_hist_entry(hist_entry(state, front.actions, returns));
        }

        torch::Tensor transform_for_prediction(torch::Tensor state, bool update_stack = true) {
            if (stack_history) {
                if (update_stack) {
                    update_prev_states(prev_states_pred, state);
                }
                return stack_prev_states(prev_states_pred, state);
            }
            return transform_state(state);
        }

        virtual void add_hist_entry(hist_entry entry) = 0;
        virtual bool is_batch_ready() = 0;
        virtual hist_entry next_batch() = 0;

    private:
        torch::Tensor transform_state(torch::Tensor state) {
            torch::Tensor x = state.clone();

            x[0] /= 1000;
            for (int i = 1; i <= 3; ++i) {
                x[i] /= 10000;
            }

            return x;
        }

        torch::Tensor stack_prev_states(std::deque<torch::Tensor> prev, torch::Tensor next_state) {
            std::vector<torch::Tensor> stack = std::vector<torch::Tensor>(prev.begin(), prev.end());
            stack.push_back(transform_state(next_state));

            return torch::cat(stack, stack_dim);
        }

        void update_prev_states(std::deque<torch::Tensor> prev, torch::Tensor state) {
            state = transform_state(state);

            //init
            if (prev.size() == 0) {
                for (int i = 0; i < (stack_size - 1); ++i) {
                    prev.push_back(state);
                }
                return;
            }

            prev.push_back(state);
            prev.pop_front();
        }

        std::deque<hist_entry> n_step_buffer;
        std::deque<torch::Tensor> prev_states, prev_states_pred;
        TReturns returns_func;
    };


    template <int NSteps, typename TReturns>
    class NextStateBatcher : public BaseBatcher<NSteps, TReturns> {
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
    class ReplayBufferBatcher : public BaseBatcher<NSteps, TReturns> {
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