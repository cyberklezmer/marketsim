#include <torch/torch.h>
#include <boost/math/constants/constants.hpp>


torch::Tensor normal_log_proba(torch::Tensor x, torch::Tensor mu, torch::Tensor std) {
    auto variance = std.pow(2);
    auto log_scale = std.log();
    auto subs = (-(x - mu).pow(2) / (2 * variance));
    return subs - log_scale - std::log(std::sqrt(2 * boost::math::constants::pi<double>()));
}

torch::Tensor normal_sample(torch::Tensor mus, torch::Tensor stds) {
    auto rand_norm = torch::randn_like(mus);
    return stds * rand_norm + mus;
}