#pragma once
#include "database_service.h"
#include "logger.h"
#include "config_manager/config_manager.h"

#include <boost/asio.hpp>
#include <boost/program_options.hpp>



namespace bpo = boost::program_options;
namespace su = SmartHome::Utils;

namespace SmartHomeDB {
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


    /**
     * @brief Loads logger configuration values from YAML file.
     *
     * @details Reads Database service logger configuration from YAML file and updates provided config struct.
     *          Existing values in struct are overwritten with values from the YAML file if present.
     *
     * @param configManager Configuration manager instance for YAML file handling.
     * @param config Logger configuration struct to be updated.
     */
    void loadLoggerYamlConfig(su::ConfigManager &configManager, su::Logger::Config &config);

    /**
     * @brief Loads configuration values from YAML file.
     *
     * @details Reads Database service configuration from YAML file and updates provided config structs.
     *          Existing values in structs are overwritten with values from the YAML file if present.
     *
     * @param configManager Configuration manager instance for YAML file handling.
     * @param databaseServiceConfig Database service launch configuration struct to be updated.
     */
    void loadYamlConfigs(su::ConfigManager &configManager,
                         DatabaseService::Config &databaseServiceConfig);

    /**
     * @brief Overwrites configurations with command-line options.
     *
     * @details Command-line arguments override YAML configuration values.
     *          This allows users to change settings at runtime without editing config files.
     *
     * @param vm Variables map containing parsed command-line options.
     * @param logTmpOpt Temporary logger options from initial parsing.
     * @param databaseServiceConfig Database service configuration to update.
     * @param loggerConfig Logger configuration to update.
     */
    void overwriteConfigsWithProgramOptions(const bpo::variables_map &vm,
                                            const loggerTemporaryOptions &logTmpOpt,
                                            DatabaseService::Config &databaseServiceConfig,
                                            su::Logger::Config &loggerConfig);

    /**
     * @brief Loads all configurations from YAML and command-line.
     *
     * @details Main configuration loading function that:
     *          1. Sets temporary logger settings from command-line
     *          2. Loads YAML configuration file
     *          3. Overwrites YAML values with command-line options
     *          4. Overwrites command-line values with environment variables
     *          5. Applies final configuration to logger
     *
     * @param vm Variables map containing parsed command-line options.
     * @param parsed Parsed options structure for counting repeated flags (e.g., -vvv).
     * @param logger Logger instance to configure and use for loading messages.
     * @param databaseServiceConfig Database service configuration struct to fill.
     *
     * @note Priority order: defaults -> YAML -> command-line -> environment variables.
     */
    void loadConfigs(const bpo::variables_map &vm,
                     const bpo::parsed_options &parsed,
                     const std::shared_ptr<su::Logger> &logger,
                     DatabaseService::Config &databaseServiceConfig);


    // Default paths for db-service
    static constexpr std::string_view sDEFAULT_CONFIG_PATH = "/etc/smarthome/db-service.yaml";
    static constexpr std::string_view sDEFAULT_LOGFILE_PATH = "/var/log/smarthome/db-service.log";

}
