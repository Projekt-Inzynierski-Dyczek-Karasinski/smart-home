#include "config_manager.h"

#include <iostream>

#include <yaml-cpp/yaml.h>

namespace SmartHome::Utils {
    ConfigManager &ConfigManager::Instance() {
        static ConfigManager ConfigInstance;
        return ConfigInstance;
    }

    ConfigManager::ConfigManager() = default;

    ConfigManager::~ConfigManager() = default;

    bool ConfigManager::loadConfig(const std::string &configPath) {
        try {
            // Try to load config file
            mConfigNode = YAML::LoadFile(configPath);
            mIsConfigLoaded.store(true);
            return true;
        } catch (YAML::Exception &e) {
            // Handle loading error
            std::cerr << "Load config error: " << e.what() << std::endl;
            return false;
        }
    };
}
