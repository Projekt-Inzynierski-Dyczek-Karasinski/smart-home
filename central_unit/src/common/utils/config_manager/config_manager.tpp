#pragma once

#include <iostream>

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

namespace SmartHome::Utils {
    template<typename T>
    std::optional<T> ConfigManager::getValue(const std::string &valuePath) {
        if (mIsConfigLoaded) {
            // Prepare keys from string
            std::vector<std::string> keys = {};
            boost::split(keys, valuePath, boost::is_any_of("."));

            std::string currentValuePath;
            YAML::Node currentNode = YAML::Clone(mConfigNode);
            // Iterate on keys, check if they are defined
            for (auto &key: keys) {
                currentValuePath += key + ".";
                currentNode = currentNode[key];

                if (!currentNode.IsDefined()) {
                    // Handle invalid path
                    if (!currentValuePath.empty()) currentValuePath.pop_back();
                    mpLogger->errorf("[CONFIG_MANAGER] Could not find %s value", currentValuePath.c_str());
                    return std::nullopt;
                }
            }
            //Return nullopt on null yaml value
            if (currentNode.IsNull()) return std::nullopt;

            // Catch value conversion errors
            try {
                return currentNode.as<T>();
            } catch (const std::exception &e) {
                currentValuePath.pop_back();
                mpLogger->errorf("[CONFIG_MANAGER] Type conversion error on %s value", currentValuePath.c_str());
                return std::nullopt;
            }
        }
        // Handle config not loaded
        mpLogger->errorf("[CONFIG_MANAGER] Config file not loaded", valuePath.c_str());
        return std::nullopt;
    }

    template<typename T>
    void ConfigManager::getValue(const std::string &valuePath, T &value) {
        auto result = getValue<T>(valuePath);
        if (result.has_value()) {
            value = result.value();
        }
    }
}
