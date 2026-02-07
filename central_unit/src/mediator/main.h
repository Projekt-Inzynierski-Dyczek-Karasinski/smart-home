#pragma once
#include "logger.h"
#include "mediator.h"
#include "config_manager/config_manager.h"

#include <boost/asio.hpp>
#include <boost/program_options.hpp>


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

    /**
     * @brief Parse MAC address string to byte array.
     *
     * @param macStr MAC address in format "XX:XX:XX:XX:XX:XX".
     *
     * @return 6-byte MAC address array, or std::nullopt on invalid format.
     *
     * @note Whitespace is ignored. String must be exactly 17 characters after stripping whitespace.
     */
    std::optional<std::array<uint8_t, 6> > parseMacAddress(const std::string &macStr);

    /**
     * @brief Read MAC address for network interface from sysfs.
     *
     * @param interface Network interface name (e.g., "eth0", "wlan0").
     *
     * @return 6-byte MAC address array, or std::nullopt if interface not found or read fails.
     *
     * @note Reads from /sys/class/net/[interface]/address.
     */
    std::optional<std::array<uint8_t, 6> > getMacAddressForInterface(const std::string &interface);

    /**
     * @brief Get MAC address of first available network interface.
     *
     * @details Iterates through /sys/class/net, skipping loopback interface,
     *          and returns MAC address of first valid interface found.
     *
     * @return 6-byte MAC address array, or std::nullopt if no valid interface found.
     */
    std::optional<std::array<uint8_t, 6> > getDefaultMacAddress();

    /**
     * @brief Loads logger configuration values from YAML file.
     *
     * @details Reads Mediator logger configuration from YAML file and updates provided config struct.
     *          Existing values in struct are overwritten with values from the YAML file if present.
     *
     * @param configManager Configuration manager instance for YAML file handling.
     * @param config Logger configuration struct to be updated.
     */
    void loadLoggerYamlConfig(su::ConfigManager &configManager, su::Logger::Config &config);

    /**
     * @brief Loads configuration values from YAML file.
     *
     * @details Reads Mediator configuration from YAML file and updates provided config structs.
     *          Existing values in structs are overwritten with values from the YAML file if present.
     *
     * @param configManager Configuration manager instance for YAML file handling.
     * @param mediatorConfig Mediator launch configuration struct to be updated.
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
     * @param mediatorConfig Mediator configuration to update.
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

}
