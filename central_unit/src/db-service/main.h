#pragma once
#include "database_service.h"
#include "logger.h"
#include "config_manager/config_manager.h"

#include <termios.h>

#include <boost/asio.hpp>
#include <boost/program_options.hpp>


namespace bpo = boost::program_options;
namespace su = SmartHome::Utils;

namespace SmartHomeDB {
    /**
     * @brief RAII helper that disables terminal echo for secure password input.
     *
     * @details On construction the object captures current terminal settings and
     *          disables echoing so passwords can be entered without being printed to the terminal.
     *          On destruction the original terminal settings are restored.
     */
    class TerminalPasswordGuard {
        termios mOldSettings{};

    public:
        /**
         * @brief Disable terminal echo and save current settings.
         */
        TerminalPasswordGuard();

        /**
         * @brief Restore previously saved terminal settings.
         */
        ~TerminalPasswordGuard();
    };

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
     * @brief Read and apply environment overrides for database configuration.
     *
     * @details Checks environment variables and overwrites corresponding fields in \p databaseServiceConfig when present.
     *          Recognized variables:
     *              - `SH_DB_HOST` -> dbHost
     *              - `SH_DB_PORT` -> dbPort (parsed as integer)
     *              - `SH_DB_NAME` -> dbName
     *              - `SH_DB_USER` -> dbUser
     *              - `SH_DB_PASSWORD` -> dbPassword
     * @details Parsing failures (e.g. non-numeric port) are logged via \p pLogger.
     *
     * @param databaseServiceConfig Configuration struct to modify with environment values.
     * @param pLogger Logger used to report parse errors and informational messages.
     */
    void overWriteConfigsWithEnvironmentVariables(DatabaseService::Config &databaseServiceConfig,
                                                  const std::shared_ptr<su::Logger> &pLogger);

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
     *          3. Overwrites YAML values with environment variables
     *          4. Overwrites environment variables values with command-line options
     *          5. Applies final configuration to logger
     *
     * @param vm Variables map containing parsed command-line options.
     * @param parsed Parsed options structure for counting repeated flags (e.g., -vvv).
     * @param pLogger Logger instance to configure and use for loading messages.
     * @param databaseServiceConfig Database service configuration struct to fill.
     *
     * @note Priority order: defaults -> YAML -> environment variables -> command-line.
     */
    void loadConfigs(const bpo::variables_map &vm,
                     const bpo::parsed_options &parsed,
                     const std::shared_ptr<su::Logger> &pLogger,
                     DatabaseService::Config &databaseServiceConfig);
}
