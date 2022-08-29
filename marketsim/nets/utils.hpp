#ifndef NET_UTILS_HPP_
#define NET_UTILS_HPP_

#include "marketsim.hpp"
#include "nets/actions.hpp"
#include <torch/torch.h>

namespace marketsim {
    struct hist_entry {
        hist_entry(torch::Tensor state, action_container<torch::Tensor> actions) :
            state(state), actions(actions){}

        torch::Tensor state;
        action_container<torch::Tensor> actions;
    };

    torch::Tensor tanh_activation(const torch::Tensor& t) {
        return torch::tanh(t);
    }

    torch::Tensor softplus_activation(const torch::Tensor& t) {
        return torch::nn::functional::softplus(t);
    }

    std::vector<torch::Tensor> get_past_states(const std::vector<hist_entry>& history, int start, int steps) {
        int idx = history.size() - start - steps;
        std::vector<torch::Tensor> states;

        for (int i = idx; i < idx + steps; ++i) {
            states.push_back(history.at(i).state);
        }

        return states;
    }

    torch::Tensor stack_state_history(std::vector<torch::Tensor>& history, int axis) {
        return torch::cat(history, axis);
    }

    torch::Tensor stack_state_history(std::vector<torch::Tensor>& history, torch::Tensor next_state, int axis) {
        history.push_back(next_state);
        return stack_state_history(history, axis);
    }

    bool bid_defined(tprice bid) { return bid != klundefprice; }
    bool ask_defined(tprice ask) { return ask != khundefprice; }

    template <tprice finitprice = 100>
    tprice get_p(const tmarketinfo& mi) {
        tprice a = mi.alpha();
        tprice b = mi.beta();
        return (bid_defined(b) && ask_defined(a)) ? (a + b) / 2 : finitprice;
    }

    template <tprice finitprice = 100>
    std::pair<tprice, tprice> get_alpha_beta(const tmarketinfo& mi, tprice last_bid, tprice last_ask)
    {
        tprice a = mi.alpha();
        tprice b = mi.beta();
        double p = get_p<finitprice>(mi);
        
        if (!bid_defined(b)) b = (!bid_defined(last_bid)) ? p - 1 : last_bid;
        if (!ask_defined(a)) a = (!ask_defined(last_ask)) ? p + 1 : last_ask;

        return std::make_pair<>(a, b);
    }

    template<int volume = 1, bool erase = true>
    trequest construct_order(tprice bid, tprice ask, tprice cons) {
        tpreorderprofile pp;
        trequest ord;

        if (bid_defined(bid)) {
            ord.addbuylimit(bid, volume);
            pp.B.add(tpreorder(bid, volume));
        }

        if (ask_defined(ask)) {
            ord.addselllimit(ask, volume);
            pp.A.add(tpreorder(ask, volume));
        }

        if (cons > 0) {
            ord.setconsumption(cons);
        }

        if (erase) {
            return {pp, trequest::teraserequest(true), cons};
        }
        return ord;
    }

    void print_state(const tmarketinfo& mi, tprice bid, tprice ask, tprice cons, int bpred = 0, int apred = 0) {
        if (bid_defined(bid)) {
            std::cout << "Bid: " << bid << " (" << bpred << "), ";
        }
        if (ask_defined(ask)) {
            std::cout << "Ask: " << ask << " (" << apred << "), ";
        }
        std::cout << "Cons: " << cons << std::endl;
        
        tprice m = mi.mywallet().money();
        tprice s = mi.mywallet().stocks();
        std::cout << "Wallet: " << m << ", Stocks: " << s << ", Value: " << m + s * mi.beta() << std::endl;
    }

}

#endif  //NET_UTILS_HPP_