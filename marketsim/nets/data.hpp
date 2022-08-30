#ifndef NET_DATA_HPP_
#define NET_DATA_HPP_

#include "nets/utils.hpp"
#include <torch/torch.h>
#include "marketsim.hpp"


namespace marketsim {

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
            if (n_step_buffer.size() < NSteps) {
                return;
            }

            torch::Tensor returns = returns_func.compute_returns(n_step_buffer, next_state);
            
            hist_entry front = n_step_buffer.front();
            add_hist_entry(hist_entry(front.state, front.actions, returns));
        }

        virtual void add_hist_entry(hist_entry entry) = 0;
        virtual hist_entry next_batch() = 0;

    private:
        std::deque<hist_entry> n_step_buffer;
        TReturns returns_func;
    };


    template <int NSteps, typename TReturns>
    class NextStateBatcher : BaseBatcher<NSteps, TReturns> {
    public:
        NextStateBatcher() : BaseBatcher<NSteps, TReturns>(), next_entry(hist_entry::empty_entry()) {}
            void add_hist_entry(hist_entry entry) {
                next_entry = entry;
            }

            virtual hist_entry next_batch() {
                return hist_entry(next_entry.state.view({1, -1}), actions_view(next_entry.actions, {1, -1}),
                                  next_entry.returns.view({1, -1}));
            }
        

    private:
        hist_entry next_entry;
    };

    template <int NSteps, typename TReturns, int batch_size>
    class ReplayBufferBatcher : BaseBatcher<NSteps, TReturns> {
    public:
        ReplayBufferBatcher() : BaseBatcher<NSteps, TReturns>() {}

        virtual void add_hist_entry(hist_entry entry) {
            buffer.push_back(entry);
        }

        virtual hist_entry next_batch() {
            std::vector<int> batch_ids;
            for (size_t i = 0; i < buffer.size(); ++i) {
                batch_ids.push_back(random_int_from_tensor(0, buffer.size()));
            }

            std::vector<hist_entry> batch;
            std::transform(batch_ids.begin(), batch_ids.end(), std::back_inserter(batch), [&](int i){ return buffer[i]; });
            
            //TODO batch states etc
            //return actions_batch(buffer);  //TODO select indices
        }
    
    private:
        std::vector<hist_entry> buffer;
    };

}

#endif  //NET_DATA_HPP_