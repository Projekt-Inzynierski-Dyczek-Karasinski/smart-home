#pragma once

#include <iostream>
#include <optional>
#include <string>

#include <yaml-cpp/yaml.h>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

namespace SmartHome::Utils {
    /**
     * @brief YAML configuration manager for smart home system
     *
     * @details Thread-safe singleton allowing type-safe access to
     *          configuration values using dot notation.
     */
    class Config {
    public:
        /**
         * @brief Get singleton instance of Config
         *
         * @details Thread-safe initialization using static local variable.
         *          Instance is created on first call and reused after.
         *
         * @return Reference to Config singleton instance
         */
        static Config &Instance();

        // Prevent copying
        Config(const Config &) = delete;

        // Prevent assignment
        Config &operator=(const Config &) = delete;

        /**
         * @brief Loads config file from YAML file
         *
         * @param configPath Path to YAML configuration file
         * @return true if loaded successfully, false on error
         */
        bool loadConfig(const std::string &configPath);

        /**
         * @brief Get typed value from configuration
         *
         * @details Retrieves values using dot notation path.
         *          Creates a copy of config node to avoid modifying original config.
         *
         * @tparam T Target type (must be YAML compatible)
         * @param valuePath Dot separated path to value ("root.branch.leaf")
         * @return Value if found, nullopt otherwise
         *
         * @code
         *  auto item = config.getValue<int>("root.branch.leaf");
         * @endcode
         */
        template<typename T>
        std::optional<T> getValue(const std::string &valuePath) {
            if (mConfigLoaded.load() == true) {
                // Prepare keys from string
                std::vector<std::string> keys = {};
                boost::split(keys, valuePath, boost::is_any_of("."));

                std::string currentValuePath = "";
                YAML::Node currentNode = YAML::Clone(mConfigNode);
                // Iterate on keys, check if they are defined
                for (auto &key: keys) {
                    currentValuePath += key + ".";
                    currentNode = currentNode[key];

                    if (currentNode.IsDefined() == false) {
                        // Handle invalid path
                        if (currentValuePath != "") currentValuePath.pop_back();
                        std::cerr << "Config get value error: value path \"" << currentValuePath << "\" is not defined"
                                << std::endl;
                        return std::nullopt;
                    }
                }
                return currentNode.as<T>();
            }
            // Handle config not loaded
            std::cerr << "Config get value error: config not loaded" << std::endl;
            return std::nullopt;
        }

        /**
        * @brief Get value and store in reference
        *
        * @details Overload that modifies parameter only if value exists
        *          Original value is preserved if path or value not found.
        *
        * @tparam T Target type (must be YAML compatible)
        * @param valuePath Dot separated path to value ("root.branch.leaf")
        * @param value Reference to store result (unchanged if not found)
        *
        * @code
        *  int port = 43321; //default value
        *  config.getValue("server.port", port); //port updated if found
        * @endcode
        */
        template<typename T>
        void getValue(const std::string &valuePath, T &value) {
            auto result = getValue<T>(valuePath);
            if (result.has_value()) {
                value = result.value();
            }
        }

    private:
        /**
         * @brief Private constructor for singleton pattern
         */
        Config();

        /**
         * @brief Private destructor for singleton pattern
         */
        ~Config();

        YAML::Node mConfigNode; ///< YAML configuration file root node
        std::atomic<bool> mConfigLoaded{false}; ///< Configuration loaded state
    };
}
