#include "strategies.hpp"
#include <torch/torch.h>

class net_base;

struct QNet : torch::nn::Module
{
    QNet(unsigned input_size, unsigned hidden_size) {
        lin1 = register_module("lin1", torch::nn::Linear(input_size, hidden_size));
        lin2 = register_module("lin2", torch::nn::Linear(hidden_size, 2));
    }

    torch::Tensor forward(torch::Tensor x) {

    x = torch::relu(fc1->forward(x));
    x = fc2->forward(x);
    
    return x;
  }

    torch::nn::Linear lin1{nullptr}, lin2{nullptr};
};


// net gets state as input, outputs action
// where is the reward???

class q_network : public net_base
{
public:
    q_network(const std::string& name, tvolume delta, ttime timeinterval, const int _max_hist,
              unsigned hidden_size, float lr)
        : net_base(name, delta, timeinterval, _max_hist),
    {
        _net = std::make_shared(_max_hist, hidden_size)
        _optim = torch::optim::Adam{net->parameters(), /*lr=*/lr};
    }

private:
    virtual bool buy_or_sell(int x, std::vector<int>& hist, int t)
    {
        return false;
    }
    std::shared_ptr<QNet> _net;
    torch::optim::Adam _optim;
}


class net_base : public tsimplestrategy
{
public:
    net_base(const std::string& name, tvolume delta, ttime timeinterval, const int _max_hist)
        : tsimplestrategy(name, timeinterval), max_hist(_max_hist), fdelta(delta)
    {}
private:
    virtual tsimpleorderprofile simpleevent(const tmarketinfo& mi, const ttradinghistory& th)
    {
        if(mi.t > max_hist * timeinterval())
        {
            std::vector<double> hist;
            for (int i = 0; i < max_hist; i++)
            {
                double p = mi.history.p(mi.t - i * timeinterval()) - mi.history.p(mi.t - (i + 1) * timeinterval());
                if (abs(p) > std::numeric_limits<double>::epsilon())
                    hist.push_back(p < 0 ? -1 : 1);
                else
                    hist.push_back(0);
            }
            std::reverse(hist.begin(), hist.end());

            int x = th.wallet().stocks();
            int t = int(mi.t);

            return buy_or_sell(x, hist, t) ? buymarket(fdelta) : sellmarket(fdelta);
        }
        return noorder();
    }

    // true means buy, false means sell
    virtual bool buy_or_sell(int x, std::vector<int>& hist, int t);

    tvolume fdelta;
    //TSpace_d P;
    const int max_hist;
};