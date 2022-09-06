#ifndef MSNEURAL_NETS_QNET_HPP_
#define MSNEURAL_NETS_QNET_HPP_

#include <torch/torch.h>


namespace marketsim {
    template <int state_size, int hidden_size, int action_size, typename TLayer>
    class QNet : public torch::nn::Module {
    public:
        QNet() {
            state_layer = register_module("state_layer", TLayer(state_size, hidden_size));
            out = register_module("out", torch::nn::Linear(hidden_size, action_size));  //TODO discrete action
        }

        torch::Tensor forward(torch::Tensor x) {
            x = state_forward(this->state_layer, x);
            x = torch::relu(x);
            return out->forward(x);
        }

    private:
        TLayer state_layer{nullptr};    
        torch::nn::Linear out{nullptr};
    };


}


#endif  //MSNEURAL_NETS_QNET_HPP_