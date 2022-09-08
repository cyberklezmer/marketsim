#ifndef MSNEURAL_CONFIG_HPP_
#define MSNEURAL_CONFIG_HPP_

#include <filesystem>
#include "marketsim.hpp"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

namespace marketsim {
    using configtree = boost::property_tree::ptree;
        
    configtree read_config(std::string path) {
        configtree pt;
        boost::property_tree::ini_parser::read_ini(path, pt);

        return pt;
    }


    struct layer_config {
        bool lstm;
        int state_size;
        int hidden_size;
        int critic_size;
    };

    layer_config read_layer_config(configtree cfg) {
        layer_config lcfg;

        lcfg.lstm = cfg.get<bool>("Layer.lstm", false);
        lcfg.state_size = cfg.get<int>("Layer.state_size", 4);
        lcfg.hidden_size = cfg.get<int>("Layer.hidden_size", 256);
        lcfg.critic_size = cfg.get<int>("Layer.critic_size", 1);

        if (cfg.get<bool>("Model.qnet", false)) {
            bool train_cons = cfg.get<bool>("Strategy.train_cons", false);
            int action_size = 2 * cfg.get<int>("Actions.spread_lim", 5) + 1;
            int cons_size = cfg.get<int>("Actions.cons_parts", 4) + 1;

            lcfg.critic_size = action_size * action_size;
            if (train_cons) {
                lcfg.critic_size *= cons_size;
            }
        }

        return lcfg;
    }

    struct action_config {
        bool discrete;
        int action_size;

        int action_offset = 0;
        int action_mult = 1;

        double output_scale = 1;
        double mu_scale = 1;
    };

    action_config read_action_config(configtree cfg) {
        action_config acfg;
        acfg.discrete = cfg.get<bool>("Actions.discrete", true);

        if (acfg.discrete) {
            int spread_lim = cfg.get<int>("Actions.spread_lim", 5);
            acfg.action_size = 2 * spread_lim + 1;
            acfg.action_offset = spread_lim;
        }
        else if (cfg.get<bool>("Strategy.speculator", false)) {
            acfg.action_size = 3;
        }
        else {
            acfg.action_size = 1;
        }

        return acfg; 
    }

    action_config read_cons_config(configtree cfg) {
        action_config acfg;
        acfg.discrete = cfg.get<bool>("Actions.discrete", true);

        if (acfg.discrete) {
            acfg.action_size = cfg.get<int>("Actions.cons_parts", 4) + 1;
            acfg.action_offset = 0;
            acfg.action_mult = cfg.get<int>("Actions.cons_step", 125);
        }
        else {
            acfg.action_size = 1;
            acfg.output_scale = cfg.get<int>("Actions.cons_mult", 100);
        }

        return acfg; 
    }


    enum batcher_type {
        replay, next_state
    };

    struct batcher_config {
        batcher_type type;
        int n_steps;
        int batch_size;
        bool clear_batch;
        int replay_buffer_max;
        int buffer_margin;

        bool stack;
        int stack_size;
        int stack_dim;
    };

    batcher_config read_batcher_config(configtree cfg) {
        batcher_config bcfg;

        std::string type = cfg.get<std::string>("Batcher.type", "next_state");
        if (type == "replay") {
            bcfg.type = batcher_type::replay;
        }
        else if (type == "next_state") {
            bcfg.type = batcher_type::next_state;
        }
        else {
            throw std::invalid_argument("Unsupported batcher type: " + type);
        }

        bcfg.n_steps = cfg.get<int>("Batcher.n_steps", 5);
        bcfg.batch_size = cfg.get<int>("Batcher.batch_size", 1);
        bcfg.clear_batch = cfg.get<bool>("Batcher.clear_batch", false);
        
        bcfg.replay_buffer_max = cfg.get<int>("Batcher.replay_buffer_max", 1000);
        bcfg.buffer_margin = cfg.get<int>("Batcher.buffer_margin", 250);

        bcfg.stack = cfg.get<bool>("Batcher.stack", false);
        bcfg.stack_size = cfg.get<int>("Batcher.stack_size", 5);
        bcfg.stack_dim = cfg.get<int>("Batcher.stack_dim", 0);
        return bcfg;
    }

    struct strategy_config {
        bool verbose;
        int volume;
        bool train_cons;
        bool use_naive_cons;
        int fixed_cons;
        int spread_lim;
        bool speculator;
    };

    strategy_config read_strategy_config(configtree cfg) {
        strategy_config scfg;

        scfg.verbose = cfg.get<bool>("Strategy.verbose", false);
        scfg.volume = cfg.get<int>("Strategy.volume", 1);
        scfg.train_cons = cfg.get<bool>("Strategy.train_cons", false);
        scfg.use_naive_cons = cfg.get<bool>("Strategy.use_naive_cons", true);
        scfg.fixed_cons = cfg.get<int>("Strategy.fixed_cons", 200);
        scfg.spread_lim = cfg.get<int>("Actions.spread_lim", 5);
        scfg.speculator = cfg.get<bool>("Strategy.speculator", false);
        
        return scfg;
    }

    struct model_config {
        double lr;
        double actor_lr;
        double critic_lr;

        bool qnet;
    };

    model_config read_model_config(configtree cfg) {
        model_config mcfg;

        mcfg.lr = cfg.get<double>("Model.lr", 0.0001);
        mcfg.actor_lr = cfg.get<double>("Model.actor_lr", 0.001);
        mcfg.critic_lr = cfg.get<double>("Model.critic_lr", 0.0001);
        mcfg.qnet = cfg.get<bool>("Model.qnet", false);
        return mcfg;
    }

    template <const char* path>
    struct neural_config {
        layer_config layer;
        action_config actions;
        action_config cons;
        batcher_config batcher;
        strategy_config strategy;
        model_config model;

        bool discrete;
        bool flags;

        static std::shared_ptr<neural_config<path>> config;
        static std::shared_ptr<neural_config<path>> load_config();
    };

    template <const char* path>
    std::shared_ptr<neural_config<path>> neural_config<path>::load_config() {
        std::cout << "Ready ready" << std::endl;
        std::cout << std::filesystem::current_path() << std::endl;

        using nc = neural_config<path>;
        std::shared_ptr<nc> config = std::make_shared<nc>();

        configtree cfg = read_config(path);
        config->layer = read_layer_config(cfg);
        config->actions = read_action_config(cfg);
        config->cons = read_cons_config(cfg);
        config->batcher = read_batcher_config(cfg);
        config->strategy = read_strategy_config(cfg);
        config->model = read_model_config(cfg);
        
        config->discrete = cfg.get<bool>("Actions.discrete", true);
        config->flags = cfg.get<bool>("Actions.flags", false);
        return config;
    }

    template <const char* path>
    std::shared_ptr<neural_config<path>> neural_config<path>::config = neural_config<path>::load_config();

}

#endif  //MSNEURAL_CONFIG_HPP_