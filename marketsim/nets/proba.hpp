#ifndef PROBA_HPP_
#define PROBA_HPP_

#include <torch/torch.h>
#include <boost/math/constants/constants.hpp>

namespace marketsim {

    torch::Tensor normal_log_proba(const torch::Tensor& x, const torch::Tensor& mu, const torch::Tensor& std) {
        //std::cout << "x, mu, std" << std::endl;
        //std::cout << x << mu << std << std::endl;

        auto variance = std.exp().pow(2);
        auto log_std = std;
        //auto variance = std.pow(2);
        //auto log_std = (std + torch::tensor({1e-12})).log();
        auto subs = (-(x - mu).pow(2) / (2 * variance));
        return subs - log_std - std::log(std::sqrt(2 * boost::math::constants::pi<double>()));
    }

    torch::Tensor normal_sample(const torch::Tensor& mus, const torch::Tensor& stds) {
        auto rand_norm = torch::randn_like(mus);
        //std::cout << "Mus, stds: " << mus << ", " << stds << std::endl;
        //return stds * rand_norm + mus;
        return stds.exp() * rand_norm + mus;
    }

    torch::Tensor sample_from_pb(const torch::Tensor& probas) {
        return at::multinomial(probas, 1);
    }
}

#endif // PROBA_HPP_