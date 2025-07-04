#include "smart_home_config.h"

#include <iostream>
#include <yaml-cpp/yaml.h>

namespace SmartHome::Utils {
    Config &Config::Instance() {
        static Config ConfigInstance;
        return ConfigInstance;
    }

    Config::Config() = default;

    Config::~Config() = default;

    bool Config::loadConfig(const std::string &configPath) {
        try {
            // Try to load config file
            mConfigNode = YAML::LoadFile(configPath);
            mConfigLoaded.store(true);
            return true;
        } catch (YAML::Exception &e) {
            // Handle loading error
            std::cerr << "Load config error: " << e.what() << std::endl;
            return false;
        }
    };


}
