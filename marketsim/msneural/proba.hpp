#ifndef MSNEURAL_PROBA_HPP_
#define MSNEURAL_PROBA_HPP_

#define _USE_MATH_DEFINES
#include <cmath>

#include <torch/torch.h>

namespace marketsim {

    double pi() {
        return M_PI;
    }

    torch::Tensor normal_log_proba(torch::Tensor x, torch::Tensor mu, torch::Tensor std) {
        auto variance = std.exp().pow(2);
        auto log_std = std;
        auto subs = (-(x - mu).pow(2) / (2 * variance));
        return subs - log_std - std::log(std::sqrt(2 * pi()));
    }

    torch::Tensor normal_sample(torch::Tensor mus, const torch::Tensor& stds) {
        auto rand_norm = torch::randn_like(mus);
        return stds.exp() * rand_norm + mus;
    }

    torch::Tensor sample_from_pb(torch::Tensor probas) {
        return torch::multinomial(probas, 1);
    }

    torch::Tensor sample_from_pb_binary(torch::Tensor proba) {
        return torch::bernoulli(proba);
    }

    torch::Tensor normal_entropy(torch::Tensor stds) {
        return torch::mean(stds + std::log(2 * pi()) / 2 + 1 / 2);
    }

    torch::Tensor logit_entropy(torch::Tensor logits) {
        auto expit = torch::exp(logits);
        return -torch::mean(expit * logits);
    }
}

#endif // MSNEURAL_PROBA_HPP_