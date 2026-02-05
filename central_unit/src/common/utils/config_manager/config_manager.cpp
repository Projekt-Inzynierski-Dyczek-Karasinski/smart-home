#include "config_manager.h"

#include <iostream>

#include <yaml-cpp/yaml.h>

namespace SmartHome::Utils {
    ConfigManager::ConfigManager(const std::shared_ptr<Logger> &logger) : mpLogger(logger) {
    }

    bool ConfigManager::loadConfig(const std::string &configPath) {
        try {
            // Try to load config file
            mConfigNode = YAML::LoadFile(configPath);
            mIsConfigLoaded = true;
            return true;
        } catch (YAML::Exception &e) {
            // Handle loading error
            mpLogger->errorf("[CONFIG_MANAGER] Load config error: %s", e.what());
            return false;
        }
    }
}
