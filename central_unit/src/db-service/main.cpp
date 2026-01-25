#include "main.h"

#include <iostream>
#include <string>

#include <boost/asio.hpp>
#include <boost/program_options.hpp>


namespace ba = boost::asio;
namespace bai = boost::asio::ip;
namespace bpo = boost::program_options;

namespace SmartHomeDB {
    void loadLoggerYamlConfig(su::ConfigManager &configManager, su::Logger::Config &config) {
        std::string root = "db-service.logging";
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

    void loadYamlConfigs(su::ConfigManager &configManager, DatabaseService::Config &databaseServiceConfig) {
        // TODO !pr


        std::string root = "db_service.ipc.tcp";
        configManager.getValue(root + ".enabled", databaseServiceConfig.tcp.isEnabled);
        configManager.getValue(root + ".address", databaseServiceConfig.tcp.endpointAddress);
        configManager.getValue(root + ".port", databaseServiceConfig.tcp.endpointPort);

        root = "db_service.ipc.uds";
        configManager.getValue(root + ".enabled", databaseServiceConfig.uds.isEnabled);
        configManager.getValue(root + ".socket_path", databaseServiceConfig.uds.endpointPath);
    }


    void overwriteConfigsWithProgramOptions(const bpo::variables_map &vm,
                                            const loggerTemporaryOptions &logTmpOpt,
                                            DatabaseService::Config &databaseServiceConfig,
                                            su::Logger::Config &loggerConfig) {
        // Logger config
        if (logTmpOpt.isVerbositySet) loggerConfig.logLevel = su::LogLevels::toLevel(logTmpOpt.verboseLevel);
        if (logTmpOpt.isQuietSet) loggerConfig.enableConsoleLogOutput = false;
        if (vm.contains("no-log-file")) loggerConfig.logFile.enabled = false;
        if (vm.contains("log-file")) loggerConfig.logFile.path = vm["log-file"].as<std::string>();
        loggerConfig.logFile.createNew = !vm["append-log"].as<bool>();
        loggerConfig.logFile.archiveOld = !vm["no-archive"].as<bool>();

        // DatabaseService config
        if (vm.contains("port")) databaseServiceConfig.tcp.endpointPort = vm["port"].as<int>();
        if (vm.contains("ipv4")) databaseServiceConfig.tcp.endpointAddress = vm["ipv4"].as<std::string>();
    }

    void loadConfigs(const bpo::variables_map &vm,
                     const bpo::parsed_options &parsed,
                     const std::shared_ptr<su::Logger> &logger,
                     DatabaseService::Config &databaseServiceConfig) {
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
        loggerConfig.logFile.path = sDEFAULT_LOGFILE_PATH;
        const std::string configPath = vm.contains("config")
                                           ? vm["config"].as<std::string>()
                                           : sDEFAULT_CONFIG_PATH.data();
        if (configManager.loadConfig(configPath)) {
            logger->debug("[MAIN_DB-SERVICE] Loading YAML logger config");
            loadLoggerYamlConfig(configManager, loggerConfig);

            logger->debug("[MAIN_DB-SERVICE] Loading YAML core config");
            loadYamlConfigs(configManager, databaseServiceConfig);
        } else {
            logger->error("[MAIN_DB-SERVICE] Could not load YAML config");
        }

        overwriteConfigsWithProgramOptions(vm, logTmpOpt, databaseServiceConfig, loggerConfig);

        logger->applyConfig(loggerConfig);
        logger->debug("[MAIN_DB-SERVICE] Smarthome database service configured");
    }
}

using namespace SmartHomeDB;

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


    //TODO !pr add cmd option for db connnection
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
    auto pLogger = std::make_shared<su::Logger>();
    auto &dbService = DatabaseService::Instance();

    su::Logger::Config loggerConfig;
    loggerConfig.logLevel = SmartHome::Utils::LogLevels::Level::DEBUG;

    pLogger->applyConfig(loggerConfig);

    DatabaseService::Config dbConfig;
    if (!dbService.initialize(dbConfig, pLogger)) {
        pLogger->critical("[MAIN_DB-SERVICE] Failed to initialize database service");
        return EXIT_FAILURE;
    }



    pLogger->info("[MAIN_DB-SERVICE] Exit");
    return EXIT_SUCCESS;
}
