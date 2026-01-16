#pragma once
#include "logger.h"
#include "config_manager/config_manager.h"

#include <boost/asio.hpp>
#include <boost/program_options.hpp>

#include "mediator.h"


namespace bpo = boost::program_options;
namespace su = SmartHome::Utils;

namespace SmartHomeMediator {
    /**
     * @brief Temporary storage for logger command-line options.
     *
     * @details Holds logger settings parsed from program options before they are merged with YAML configuration.
     */
    struct loggerTemporaryOptions {
        bool isVerbositySet = false;
        bool isQuietSet = false;

        uint8_t verboseLevel = static_cast<uint8_t>(su::LogLevels::defaultLevel);
    };

    std::optional<std::array<uint8_t, 6>> parseMacAddress(const std::string& macStr);

    std::optional<std::array<uint8_t, 6>> getMacAddressForInterface(const std::string& interface);

    std::optional<std::array<uint8_t, 6>> getDefaultMacAddress();

    /**
     * @brief Loads logger configuration values from YAML file.
     *
     * @details Reads SmartHome logger configuration from YAML file and updates provided config struct.
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
     * @param mediatorConfig Mediator launch configuration struct to be updated. //TODO
     */
    void loadYamlConfigs(su::ConfigManager &configManager,
                         Mediator::Config &mediatorConfig);

    /**
     * @brief Overwrites configurations with command-line options.
     *
     * @details Command-line arguments override YAML configuration values.
     *          This allows users to change settings at runtime without editing config files.
     *
     * @param vm Variables map containing parsed command-line options.
     * @param logTmpOpt Temporary logger options from initial parsing.
     * @param mediatorConfig Core configuration to update.
     * @param loggerConfig Logger configuration to update.
     */
    void overwriteConfigsWithProgramOptions(const bpo::variables_map &vm,
                                            const loggerTemporaryOptions &logTmpOpt,
                                            Mediator::Config &mediatorConfig,
                                            su::Logger::Config &loggerConfig);

    /**
     * @brief Loads all configurations from YAML and command-line.
     *
     * @details Main configuration loading function that:
     *          1. Sets temporary logger settings from command-line
     *          2. Loads YAML configuration file
     *          3. Overwrites YAML values with command-line options
     *          4. Applies final configuration to logger
     *
     * @param vm Variables map containing parsed command-line options.
     * @param parsed Parsed options structure for counting repeated flags (e.g., -vvv).
     * @param logger Logger instance to configure and use for loading messages.
     * @param mediatorConfig Mediator configuration struct to fill.
     *
     * @note Priority order: defaults -> YAML -> command-line.
     */
    void loadConfigs(const bpo::variables_map &vm,
                     const bpo::parsed_options &parsed,
                     const std::shared_ptr<su::Logger> &logger,
                     Mediator::Config &mediatorConfig);


    /// Default path for smarthome YAML config
    static constexpr std::string_view s_DEFAULT_CONFIG_PATH = "/etc/smarthome/mediator.yaml";
    static constexpr std::string_view s_DEFAULT_LOGFILE_PATH = "/var/log/smarthome/mediator.log";

}
