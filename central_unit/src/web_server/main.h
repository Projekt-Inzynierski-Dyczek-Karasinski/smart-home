#pragma once
#include "web_server.h"
#include "logger.h"
#include "config_manager/config_manager.h"

#include <boost/asio.hpp>
#include <boost/program_options.hpp>

namespace bpo = boost::program_options;
namespace su = SmartHome::Utils;

namespace SmartHomeWebServer {
    /**
     * @brief Temporary storage for logger command-line options.
     */
    struct LoggerTemporaryOptions {
        bool isVerbositySet = false;
        bool isQuietSet = false;
        uint8_t verboseLevel = static_cast<uint8_t>(su::LogLevels::defaultLevel);
    };

    /**
     * @brief Load logger configuration from YAML file.
     *
     * @param configManager Configuration manager instance.
     * @param config Logger config struct to update.
     */
    void loadLoggerYamlConfig(su::ConfigManager &configManager, su::Logger::Config &config);

    /**
     * @brief Load web server configuration from YAML file.
     *
     * @param configManager Configuration manager instance.
     * @param webServerConfig WebServer config struct to update.
     */
    void loadYamlConfigs(su::ConfigManager &configManager, WebServer::Config &webServerConfig);

    /**
     * @brief Read and apply environment overrides for web server configuration.
     *
     * @details Checks environment variables and overwrites corresponding fields in \p webServerConfig when present.
     *          Recognized variables:
     *              - `SH_ALLOWED_ORIGIN` -> allowedOrigin
     *
     * @param webServerConfig Configuration struct to modify with environment values.
     */
    void overwriteConfigsWithEnvironmentVariables(WebServer::Config &webServerConfig);

    /**
     * @brief Overwrite configurations with command-line options.
     *
     * @param vm Parsed command-line variables.
     * @param logTmpOpt Temporary logger options.
     * @param webServerConfig WebServer config to update.
     * @param loggerConfig Logger config to update.
     */
    void overwriteConfigsWithProgramOptions(const bpo::variables_map &vm,
                                            const LoggerTemporaryOptions &logTmpOpt,
                                            WebServer::Config &webServerConfig,
                                            su::Logger::Config &loggerConfig);

    /**
     * @brief Load all configurations.
     *
     * @details Priority: defaults -> YAML -> command-line.
     *
     * @param vm Parsed command-line variables.
     * @param parsed Parsed options for counting repeated flags.
     * @param pLogger Logger instance.
     * @param webServerConfig WebServer config struct to fill.
     */
    void loadConfigs(const bpo::variables_map &vm,
                     const bpo::parsed_options &parsed,
                     const std::shared_ptr<su::Logger> &pLogger,
                     WebServer::Config &webServerConfig);
}
