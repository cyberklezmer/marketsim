#ifndef PROBA_HPP_
#define PROBA_HPP_

#include <torch/torch.h>
#include <boost/math/constants/constants.hpp>


torch::Tensor normal_log_proba(const torch::Tensor& x, const torch::Tensor& mu, const torch::Tensor& std) {
    std::cout << x << mu << std << std::endl;

    auto variance = std.pow(2);
    auto log_std = (std + torch::tensor({1e-12})).log();
    auto subs = (-(x - mu).pow(2) / (2 * variance));
    return subs - log_std - std::log(std::sqrt(2 * boost::math::constants::pi<double>()));
}

torch::Tensor normal_sample(const torch::Tensor& mus, const torch::Tensor& stds) {
    auto rand_norm = torch::randn_like(mus);
    return stds * rand_norm + mus;
}

#endif // PROBA_HPP_