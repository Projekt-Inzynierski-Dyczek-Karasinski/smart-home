#pragma once

#include "core.h"
#include "config_manager.h"

namespace SmartHome {
    /**
     *@brief Configuration for launching and managing mediator service.
     */
    struct MediatorConfig {
        /// Enable/Disable mediator launch by Core
        bool isEnabled = false;
        /// Mode in which mediator will run
        Utils::ServiceType serviceType = Utils::ServiceType::AUTO;
        /// Path to mediator executable for standalone mode
        std::string execPath = "/usr/local/bin/smarthome-mediator";
        /// Path to mediator's own YAML config file
        std::string configPath = "/etc/smarthome/mediator.yaml";
    };

    /**
     * @brief Loads logger configuration vales from YAML file.
     *
     * @details Reads smarthome logger configuration from YAML file and updates provided config structs.
     *          Existing values in structs are overwritten with values from the YAML file if present.
     *
     * @param configManager Configuration manager instance for YAML file handling.
     * @param config Pointer to logger configuration struct to be updated.
     */
    void loadLoggerConfig(Utils::ConfigManager &configManager, Utils::Logger::Config *config);

    /**
     * @brief Loads configuration vales from YAML file.
     *
     * @details Reads smarthome configuration from YAML file and updates provided config structs.
     *          Existing values in structs are overwritten with values from the YAML file if present.
     *
     * @param configManager Configuration manager instance for YAML file handling.
     * @param coreConfig Pointer to Core configuration struct to be updated.
     * @param mediatorConfig Pointer to mediator launch configuration struct to be updated.
     */
    void loadConfigValues(Utils::ConfigManager &configManager,
                          Core::Config *coreConfig,
                          MediatorConfig *mediatorConfig);

    /// Default path for smarthome YAML config
    static constexpr const char *s_DEFAULT_CONFIG_PATH = "/etc/smarthome/smart_home.yaml";
}
