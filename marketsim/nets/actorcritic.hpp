#include <torch/torch.h>

struct ActorCriticNet : torch.nn.Module {
    ActorCriticNet() :
        state_size(20),
        action_size(20),
        hidden_size(128)
    {
        //TODO state_size?
        linear = register_module("linear", torch::nn::Linear(state_size, hidden_size))
        critic = register_module("critic", torch::nn::Linear(hidden_size, 1))
        actor = register_module("actor", torch::nn::Linear(hidden_size, action_size))
    }

    //TODO možná nepůjde pár
    std::pair<torch::Tensor, torch::Tensor> forward(torch::Tensor x) {
        x = torch::relu(linear->forward(x))
        
        auto action_prob = torch::log_softmax(actor->forward(x), /*dim=*/-1)  //TODO dim?
        auto state_values = critic->forward(x)
        return std::make_pair(action_prob, state_values)
    }

    int state_size;
    int hidden_size;
    int action_size;  //TODO zjistit jak consumption atd (viz ten clanek)
}