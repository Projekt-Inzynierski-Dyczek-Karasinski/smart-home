#pragma once

#include "config_manager/config_manager.h"
#include "logger.h"

#include <string_view>
#include <string>

namespace su = SmartHome::Utils;

namespace SmartHomeGUI {
    /**
     *@brief Configuration for GUI application.
     */
    struct GuiConfig {
        bool udsEnabled = true;
        std::string udsPath = "/var/run/smarthomed.sock";
    };

    /**
     * @brief Loads logger configuration values from YAML file.
     *
     * @details Reads SmartHomeGUI logger configuration from YAML file and updates provided config struct.
     *          Existing values in struct are overwritten with values from the YAML file if present.
     *
     * @param configManager Configuration manager instance for YAML file handling.
     * @param config Logger configuration struct to be updated.
     */
    void loadLoggerYamlConfig(su::ConfigManager &configManager, su::Logger::Config &config);

    /**
     * @brief Loads configuration values from YAML file.
     *
     * @details Reads SmartHome configuration from YAML file and updates provided config structs.
     *          Existing values in structs are overwritten with values from the YAML file if present.
     *
     * @param configManager Configuration manager instance for YAML file handling.
     * @param config Core configuration struct to be updated.
     */
    void loadGuiYamlConfig(su::ConfigManager &configManager, GuiConfig &config);

    /**
     * @brief Loads all configurations from YAML.
     *
     * @param logger Logger instance to configure and use for loading messages.
     * @param config GUI configuration struct to fill.
     */
    void loadConfigs(const std::shared_ptr<su::Logger> &logger, GuiConfig &config);

    static constexpr std::string_view s_DEFAULT_CONFIG_PATH = "/etc/smarthome/smart_home.yaml";
}
