#pragma once

#include "../logger/logger.h"

#include <optional>
#include <string>

#include <yaml-cpp/yaml.h>

namespace SmartHome::Utils {
    /**
     * @brief YAML configuration manager for smart home system.
     *
     * @details Configuration manager allowing type-safe access to
     *          configuration values using dot notation.
     */
    class ConfigManager {
    public:
        /**
         * @brief ConfigManager constructor assigning logger instance reference.
         *
         * @param logger Logger shared pointer instance reference.
         */
        explicit ConfigManager(const std::shared_ptr<Logger> &logger);

        /**
         * @brief Loads config file from YAML file.
         *
         * @param configPath Path to YAML configuration file.
         * @return true if loaded successfully, false on error.
         */
        bool loadConfig(const std::string &configPath);

        /**
         * @brief Get typed value from configuration.
         *
         * @details Retrieves values using dot notation path.
         *          Creates a copy of config node to avoid modifying original config.
         *
         * @tparam T Target type (must be YAML compatible).
         * @param valuePath Dot separated path to value ("root.branch.leaf").
         * @return Value if found, nullopt otherwise.
         *
         * @code
         *  auto item = config.getValue<int>("root.branch.leaf");
         * @endcode
         */
        template<typename T>
        std::optional<T> getValue(const std::string &valuePath);

        /**
        * @brief Get value and store in reference.
        *
        * @details Overload that modifies parameter only if value exists.
        *          Original value is preserved if path or value not found.
        *
        * @tparam T Target type (must be YAML compatible).
        * @param valuePath Dot separated path to value ("root.branch.leaf").
        * @param value Reference to store result (unchanged if not found).
        *
        * @code
        *  int port = 43321; //default value
        *  config.getValue("server.port", port); //port updated if found
        * @endcode
        */
        template<typename T>
        void getValue(const std::string &valuePath, T &value);

    private:
        YAML::Node mConfigNode; ///< YAML configuration file root node.
        bool mIsConfigLoaded = false; ///< Configuration loaded state.
        std::shared_ptr<Logger> mpLogger; ///< Logger instance reference
    };
}

#include "config_manager.tpp"
