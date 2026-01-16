#include "main.h"

#include <iostream>
#include <string>

#include <boost/asio.hpp>
#include <boost/program_options.hpp>


namespace ba = boost::asio;
namespace bai = boost::asio::ip;
namespace bpo = boost::program_options;

namespace SmartHomeMediator {
    std::optional<std::array<uint8_t, 6> > parseMacAddress(const std::string &macStr) {
        std::array<uint8_t, 6> mac;

        std::string strippedMacStr = macStr;
        std::erase_if(strippedMacStr, ::isspace);

        if (strippedMacStr.length() != 17) {
            return std::nullopt;
        }

        for (auto i = 0; i < 6; ++i) {
            const auto offset = i * 3;

            if (i < 5 && strippedMacStr[offset + 2] != ':') {
                return std::nullopt;
            }

            std::string byteStr = strippedMacStr.substr(offset, 2);
            char *end;
            const auto value = std::strtoul(byteStr.c_str(), &end, 16);

            if (end != byteStr.c_str() + 2 || value > 0xFF) {
                return std::nullopt;
            }

            mac[i] = static_cast<uint8_t>(value);
        }

        return mac;
    }

    std::optional<std::array<uint8_t, 6> > getMacAddressForInterface(const std::string &interface) {
        const std::string path = "/sys/class/net/" + interface + "/address";

        if (!std::filesystem::exists(path)) {
            return std::nullopt;
        }

        std::ifstream file(path);
        if (!file.is_open()) {
            return std::nullopt;
        }

        std::string macStr;
        std::getline(file, macStr);

        if (macStr.empty()) {
            return std::nullopt;
        }

        return parseMacAddress(macStr);
    }

    std::optional<std::array<uint8_t, 6> > getDefaultMacAddress() {
        if (!std::filesystem::exists("/sys/class/net")) {
            return std::nullopt;
        }

        for (const auto &entry: std::filesystem::directory_iterator("/sys/class/net")) {
            std::string ifaceName = entry.path().filename().string();

            if (ifaceName == "lo") continue;

            auto mac = getMacAddressForInterface(ifaceName);
            if (mac.has_value()) {
                return mac;
            }
        }

        return std::nullopt;
    }


    void loadLoggerYamlConfig(su::ConfigManager &configManager, su::Logger::Config &config) {
        std::string root = "mediator.logging";
        config.logLevel = su::LogLevels::toLevel(
            configManager.getValue<int>(root + ".log_level").
            value_or(static_cast<int>(su::LogLevels::defaultLevel)));
        configManager.getValue(root + ".enable_console_log_output", config.enableConsoleLogOutput);

        root += ".log_file";
        configManager.getValue(root + ".enabled", config.logFile.enabled);
        configManager.getValue(root + ".create_new", config.logFile.createNew);
        configManager.getValue(root + ".archive_old", config.logFile.archiveOld);
        configManager.getValue(root + ".path", config.logFile.path);
    }

    void loadYamlConfigs(su::ConfigManager &configManager, Mediator::Config &mediatorConfig) {
        std::string root = "mediator.rf_client";
        configManager.getValue(root + ".default_mac_address", mediatorConfig.rfClient.defaultMac);
        configManager.getValue(root + ".default_rf_channel", mediatorConfig.rfClient.defaultChannel);

        root = "mediator.rf_client.rf_driver";
        configManager.getValue(root + ".uart_port_path", mediatorConfig.rfClient.rfDriverConfig.uartPortPath);
        configManager.getValue(root + ".uart_baud_rate", mediatorConfig.rfClient.rfDriverConfig.uartBaudrate);
        configManager.getValue(root + ".gpio_chip_path", mediatorConfig.rfClient.rfDriverConfig.chipPath);
        configManager.getValue(root + ".set_pin_number", mediatorConfig.rfClient.rfDriverConfig.setPin);


        root = "mediator.ipc.tcp";
        configManager.getValue(root + ".enabled", mediatorConfig.tcp.isEnabled);
        configManager.getValue(root + ".address", mediatorConfig.tcp.endpointAddress);
        configManager.getValue(root + ".port", mediatorConfig.tcp.endpointPort);

        root = "mediator.ipc.uds";
        configManager.getValue(root + ".enabled", mediatorConfig.uds.isEnabled);
        configManager.getValue(root + ".socket_path", mediatorConfig.uds.endpointPath);
    }


    void overwriteConfigsWithProgramOptions(const bpo::variables_map &vm,
                                            const loggerTemporaryOptions &logTmpOpt,
                                            Mediator::Config &mediatorConfig,
                                            su::Logger::Config &loggerConfig) {
        // Logger config
        if (logTmpOpt.isVerbositySet) loggerConfig.logLevel = su::LogLevels::toLevel(logTmpOpt.verboseLevel);
        if (logTmpOpt.isQuietSet) loggerConfig.enableConsoleLogOutput = false;
        if (vm.contains("no-log-file")) loggerConfig.logFile.enabled = false;
        if (vm.contains("log-file")) loggerConfig.logFile.path = vm["log-file"].as<std::string>();
        loggerConfig.logFile.createNew = !vm["append-log"].as<bool>();
        loggerConfig.logFile.archiveOld = !vm["no-archive"].as<bool>();

        // Mediator config
        if (vm.contains("port")) mediatorConfig.tcp.endpointPort = vm["port"].as<int>();
        if (vm.contains("ipv4")) mediatorConfig.tcp.endpointAddress = vm["ipv4"].as<std::string>();
    }

    void loadConfigs(const bpo::variables_map &vm,
                     const bpo::parsed_options &parsed,
                     const std::shared_ptr<su::Logger> &logger,
                     Mediator::Config &mediatorConfig) {
        loggerTemporaryOptions logTmpOpt;

        // Set temporary logger config based on program options and default values before reading YAML config
        if (vm.contains("verbose")) {
            // Increase verbose level
            for (const auto &option: parsed.options) {
                if (option.string_key == "verbose") {
                    logTmpOpt.verboseLevel++;
                }
            }
            logTmpOpt.isVerbositySet = true;
        }

        if (vm.contains("quiet")) {
            logger->disableConsoleLogging();
            logTmpOpt.isQuietSet = true;
        }

        if (vm.contains("log-level")) {
            // Overwrite verbose with explicit log-level
            logTmpOpt.verboseLevel = vm["log-level"].as<uint8_t>();
            logTmpOpt.isVerbositySet = true;
        }

        if (logTmpOpt.isVerbositySet) logger->setLevel(su::LogLevels::toLevel(logTmpOpt.verboseLevel));

        // Load YAML config
        auto configManager = su::ConfigManager(logger);
        su::Logger::Config loggerConfig;
        loggerConfig.logFile.path = s_DEFAULT_LOGFILE_PATH;
        const std::string configPath = vm.contains("config")
                                           ? vm["config"].as<std::string>()
                                           : s_DEFAULT_CONFIG_PATH.data();
        if (configManager.loadConfig(configPath)) {
            logger->debug("[MAIN] Loading YAML logger config");
            loadLoggerYamlConfig(configManager, loggerConfig);

            logger->debug("[MAIN] Loading YAML core config");
            loadYamlConfigs(configManager, mediatorConfig);
        } else {
            logger->error("[MAIN] Could not load YAML config");
        }

        overwriteConfigsWithProgramOptions(vm, logTmpOpt, mediatorConfig, loggerConfig);

        logger->applyConfig(loggerConfig);
        logger->debug("[MAIN] Smarthome configured");
    }
}

using namespace SmartHomeMediator;

int main(int argc, char *argv[]) {
    // Define program options
    bpo::options_description generic("Generic options");
    generic.add_options()
            ("help,h", "Produce help message")
            ("config,c", bpo::value<std::string>(), "Path to config file")

            ("verbose,v", bpo::value<int>()->implicit_value(0)->zero_tokens()->composing(),
             "Increase verbosity/log-level (-v=WARNING,-vv=INFO,-vvv=DEBUG)")
            ("quiet,q", "Suppress console output")
            ("log-level,l", bpo::value<uint8_t>(),
             "Set verbosity/log-level (0=NONE, 1=CRITICAL 2=ERROR, 3=WARNING, 4=INFO, 5=DEBUG)")

            ("log-file", bpo::value<std::string>(), "Log file path")
            ("append-log", bpo::bool_switch()->default_value(false), "Append to existing log file")
            ("no-archive", bpo::bool_switch()->default_value(false), "Disables archiving old log files")
            ("no-log-file", "Disable file logging")

            ("port,p", bpo::value<int>(), "TCP IPC port")
            ("ipv4,4", bpo::value<std::string>(), "TCP IPC ipv4 address");


    //TODO add options
    //TODO add error handling for unknown options

    // Handle options
    bpo::variables_map vm;
    bpo::parsed_options parsed = bpo::parse_command_line(argc, argv, generic);
    bpo::store(parsed, vm);
    bpo::notify(vm);

    // Print help in cmd on help option
    if (vm.contains("help")) {
        std::cout << generic << std::endl;
        return 0;
    }

    // Define instances
    auto &mediator = Mediator::Instance();
    auto logger = std::make_shared<su::Logger>();

    // Define config struct with default values
    Mediator::Config mediatorConfig;
    const auto defaultMac = getDefaultMacAddress();
    if (defaultMac.has_value()) mediatorConfig.rfClient.defaultMac = defaultMac.value();


    loadConfigs(vm, parsed, logger, mediatorConfig);

    if (!mediator.initialize(mediatorConfig, logger)) {
        logger->critical("[MAIN] Failed to initialize");
        return EXIT_FAILURE;
    }

    mediator.run();

    logger->info("[MAIN] Exit");
    return EXIT_SUCCESS;
}
