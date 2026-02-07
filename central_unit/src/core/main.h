#pragma once

#include "core.h"
#include "config_manager/config_manager.h"
#include "constants.h"

#include <boost/process.hpp>
#include <boost/program_options.hpp>

namespace bp = boost::process;
namespace bpo = boost::program_options;

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
        std::string execPath = std::string{Constants::DefaultPaths::MEDIATOR_EXEC};
        /// Path to mediator's own YAML config file
        std::string configPath = std::string{Constants::DefaultPaths::MEDIATOR_CONFIG};
    };

    /**
     * @brief Temporary storage for logger command-line options.
     *
     * @details Holds logger settings parsed from program options before they are merged with YAML configuration.
     */
    struct loggerTemporaryOptions {
        bool isVerbositySet = false;
        bool isQuietSet = false;

        uint8_t verboseLevel = static_cast<uint8_t>(Utils::LogLevels::defaultLevel);
    };


    /**
     * @brief Loads logger configuration values from YAML file.
     *
     * @details Reads SmartHome logger configuration from YAML file and updates provided config struct.
     *          Existing values in struct are overwritten with values from the YAML file if present.
     *
     * @param configManager Configuration manager instance for YAML file handling.
     * @param config Logger configuration struct to be updated.
     */
    void loadLoggerYamlConfig(Utils::ConfigManager &configManager, Utils::Logger::Config &config);

    /**
     * @brief Loads configuration values from YAML file.
     *
     * @details Reads SmartHome configuration from YAML file and updates provided config structs.
     *          Existing values in structs are overwritten with values from the YAML file if present.
     *
     * @param configManager Configuration manager instance for YAML file handling.
     * @param coreConfig Core configuration struct to be updated.
     * @param mediatorConfig Mediator launch configuration struct to be updated.
     */
    void loadYamlConfigs(Utils::ConfigManager &configManager,
                         Core::Config &coreConfig,
                         MediatorConfig &mediatorConfig);

    /**
     * @brief Overwrites configurations with command-line options.
     *
     * @details Command-line arguments override YAML configuration values.
     *          This allows users to change settings at runtime without editing config files.
     *
     * @param vm Variables map containing parsed command-line options.
     * @param logTmpOpt Temporary logger options from initial parsing.
     * @param coreConfig Core configuration to update.
     * @param loggerConfig Logger configuration to update.
     */
    void overwriteConfigsWithProgramOptions(const bpo::variables_map &vm,
                                            const loggerTemporaryOptions &logTmpOpt,
                                            Core::Config &coreConfig,
                                            Utils::Logger::Config &loggerConfig);

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
     * @param coreConfig Core configuration struct to fill.
     * @param mediatorConfig Mediator configuration struct to fill.
     *
     * @note Priority order: defaults -> YAML -> command-line.
     */
    void loadConfigs(const bpo::variables_map &vm,
                     const bpo::parsed_options &parsed,
                     const std::shared_ptr<Utils::Logger> &logger,
                     Core::Config &coreConfig,
                     MediatorConfig &mediatorConfig);



    static constexpr auto s_MAX_RETRIES = 10;
    static constexpr auto s_RETRY_DELAY = 250ms;
}
