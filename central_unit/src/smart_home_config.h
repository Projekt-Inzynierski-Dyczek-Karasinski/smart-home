#ifndef SMART_HOME_CONFIG_H
#define SMART_HOME_CONFIG_H
#include <iostream>
#include <optional>
#include <string>
#include <yaml-cpp/yaml.h>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>


namespace SmartHome::Utils {
    class Config {
    public:
        static Config &Instance();

        Config(const Config &) = delete;

        Config &operator=(const Config &) = delete;

        bool loadConfig(const std::string &configPath);

        template<typename T>
        std::optional<T> getValue(const std::string &valuePath) {
            if (mConfigLoaded.load() == true) {
                std::vector<std::string> keys = {};
                boost::split(keys, valuePath, boost::is_any_of("."));

                std::string currentValuePath = "";
                YAML::Node currentNode = YAML::Clone(mConfigNode);
                for (auto &key: keys) {
                    currentValuePath += key + ".";
                    currentNode = currentNode[key];

                    if (currentNode.IsDefined() == false) {
                        if (currentValuePath != "") currentValuePath.pop_back();
                        std::cerr << "Config get value error: value path \"" << currentValuePath << "\" is not defined"
                                << std::endl;
                        return std::nullopt;
                    }
                }
                return currentNode.as<T>();
            }
            std::cerr << "Config get value error: config not loaded" << std::endl;
            return std::nullopt;
        }

        template<typename T>
        void getValue(const std::string &valuePath, T &value) {
            auto result = getValue<T>(valuePath);
            if (result.has_value()) {
                value = result.value();
            }
        }


    private:
        Config();

        ~Config();

        YAML::Node mConfigNode;

        std::atomic<bool> mConfigLoaded{false};
    };
}


#endif //SMART_HOME_CONFIG_H
