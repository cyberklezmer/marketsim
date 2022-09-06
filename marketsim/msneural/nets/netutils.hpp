#ifndef MSNEURAL_NETS_NETUTILS_HPP_
#define MSNEURAL_NETS_NETUTILS_HPP_

#include <torch/torch.h>
using namespace torch::indexing;  


namespace marketsim {
    torch::Tensor state_forward(torch::nn::LSTM layer, torch::Tensor x) {
        if (x.dim() == 2) {
            x = x.unsqueeze(1);
        }

        auto res = layer->forward(x);
        torch::Tensor out = std::get<0>(res);
        
        // get the last output
        out = out.index({Slice(), out.sizes()[1] - 1});
        
        return out;
    }

    torch::Tensor state_forward(torch::nn::Linear layer, torch::Tensor x) {
        return layer->forward(x);
    } 

}


#endif  //MSNEURAL_NETS_NETUTILS_HPP_