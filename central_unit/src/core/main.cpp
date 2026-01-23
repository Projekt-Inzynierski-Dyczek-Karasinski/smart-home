#include "main.h"
#include "core.h"
#include "logger.h"

#include <iostream>
#include <filesystem>

using namespace std::chrono_literals;

namespace SmartHome {
    void loadLoggerYamlConfig(Utils::ConfigManager &configManager, Utils::Logger::Config &config) {
        std::string root = "core.logging";
        config.logLevel = Utils::LogLevels::toLevel(
            configManager.getValue<int>(root + ".log_level").
            value_or(static_cast<int>(Utils::LogLevels::defaultLevel)));
        configManager.getValue(root + ".enable_console_log_output", config.enableConsoleLogOutput);

        root += ".log_file";
        configManager.getValue(root + ".enabled", config.logFile.enabled);
        configManager.getValue(root + ".create_new", config.logFile.createNew);
        configManager.getValue(root + ".archive_old", config.logFile.archiveOld);
        configManager.getValue(root + ".path", config.logFile.path);
    }

    void loadYamlConfigs(Utils::ConfigManager &configManager, Core::Config &coreConfig,
                         MediatorConfig &mediatorConfig) {
        // Mediator config
        std::string root = "services.mediator";
        configManager.getValue(root + ".enabled", mediatorConfig.isEnabled);
        mediatorConfig.serviceType = Utils::resolveServiceType(
            configManager.getValue<std::string>(root + ".service_type").value());
        configManager.getValue(root + ".exec_path", mediatorConfig.execPath);
        configManager.getValue(root + ".config_path", mediatorConfig.configPath);

        // Core config
        root = "core";
        configManager.getValue(root + ".main_threads", coreConfig.coreMainThreads);
        configManager.getValue(root + ".worker_threads", coreConfig.coreWorkerThreads);

        root = "core.ipc";
        configManager.getValue(root + ".threads", coreConfig.ipcServerThreads);

        root = "core.ipc.tcp";
        configManager.getValue(root + ".enabled", coreConfig.tcp.isEnabled);
        configManager.getValue(root + ".address", coreConfig.tcp.endpointAddress);
        configManager.getValue(root + ".port", coreConfig.tcp.endpointPort);

        root = "core.ipc.uds";
        configManager.getValue(root + ".enabled", coreConfig.uds.isEnabled);
        configManager.getValue(root + ".socket_path", coreConfig.uds.endpointPath);
    }


    void overwriteConfigsWithProgramOptions(const bpo::variables_map &vm,
                                            const loggerTemporaryOptions &logTmpOpt,
                                            Core::Config &coreConfig,
                                            Utils::Logger::Config &loggerConfig) {
        // Logger config
        if (logTmpOpt.isVerbositySet) loggerConfig.logLevel = Utils::LogLevels::toLevel(logTmpOpt.verboseLevel);
        if (logTmpOpt.isQuietSet) loggerConfig.enableConsoleLogOutput = false;
        if (vm.contains("no-log-file")) loggerConfig.logFile.enabled = false;
        if (vm.contains("log-file")) loggerConfig.logFile.path = vm["log-file"].as<std::string>();
        loggerConfig.logFile.createNew = !vm["append-log"].as<bool>();
        loggerConfig.logFile.archiveOld = !vm["no-archive"].as<bool>();

        // Core config
        if (vm.contains("port")) coreConfig.tcp.endpointPort = vm["port"].as<int>();
        if (vm.contains("ipv4")) coreConfig.tcp.endpointAddress = vm["ipv4"].as<std::string>();
    }

    void loadConfigs(const bpo::variables_map &vm,
                     const bpo::parsed_options &parsed,
                     const std::shared_ptr<Utils::Logger> &logger,
                     Core::Config &coreConfig,
                     MediatorConfig &mediatorConfig) {
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

        if (logTmpOpt.isVerbositySet) logger->setLevel(Utils::LogLevels::toLevel(logTmpOpt.verboseLevel));

        // Load YAML config
        auto configManager = Utils::ConfigManager(logger);
        Utils::Logger::Config loggerConfig;
        loggerConfig.logFile.path = s_DEFAULT_LOGFILE_PATH;
        const std::string configPath = vm.contains("config")
                                           ? vm["config"].as<std::string>()
                                           : s_DEFAULT_CONFIG_PATH.data();
        if (configManager.loadConfig(configPath)) {
            logger->debug("[MAIN] Loading YAML logger config");
            loadLoggerYamlConfig(configManager, loggerConfig);

            logger->debug("[MAIN] Loading YAML core config");
            loadYamlConfigs(configManager, coreConfig, mediatorConfig);
        } else {
            logger->error("[MAIN] Could not load YAML config");
        }

        overwriteConfigsWithProgramOptions(vm, logTmpOpt, coreConfig, loggerConfig);

        logger->applyConfig(loggerConfig);
        logger->debug("[MAIN] Smarthome configured");
    }
}

using namespace SmartHome;


int main(int argc, char *argv[]) {
    // Define program options
    bpo::options_description generic("Generic options");
    generic.add_options()
            ("help,h", "Produce help message")
            ("config,c", bpo::value<std::string>(), "Path to main config file")

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
    auto &core = Core::Instance();
    bp::child mediator;
    auto logger = std::make_shared<Utils::Logger>();

    // Define config structs with default values
    Core::Config coreConfig;
    MediatorConfig mediatorConfig;

    loadConfigs(vm, parsed, logger, coreConfig, mediatorConfig);

    // Initialize Core
    logger->debug("[MAIN] Initializing Core...");
    if (!core.initialize(coreConfig, logger)) {
        logger->critical("[MAIN] Failed to initialize Core");
        return 1;
    }
    logger->debug("[MAIN] Initialized Core");


    // TODO launch gui if in gui mode

    // Launch mediator if enabled
    if (mediatorConfig.isEnabled) {
        logger->debug("[MAIN] Initializing Mediator...");
        switch (mediatorConfig.serviceType) {
            default:
            case Utils::ServiceType::AUTO:
            case Utils::ServiceType::STANDALONE:
                if (std::filesystem::exists(mediatorConfig.execPath)) {
                    logger->info("[MAIN] Starting Mediator in STANDALONE mode");
                    mediator = bp::child(mediatorConfig.execPath, "--config", mediatorConfig.execPath);
                } else {
                    logger->error("[MAIN] Could not find mediator executable");
                }
                break;
            case Utils::ServiceType::SYSTEMD:
                logger->info("[MAIN] Mediator in SYSTEMD mode - waiting for service to be ready"); {
                    bool isRunning = false;

                    for (int attempt = 1; attempt <= s_MAX_RETRIES; ++attempt) {
                        int ret = std::system("systemctl is-active --quiet smarthome-mediator.service");
                        if (ret == 0) {
                            logger->info("[MAIN] Mediator service is running");
                            isRunning = true;
                            break;
                        }

                        if (attempt < s_MAX_RETRIES) {
                            logger->debugf("[MAIN] Mediator not ready yet, retry %d/%d", attempt, s_MAX_RETRIES);
                            std::this_thread::sleep_for(s_RETRY_DELAY);
                        }
                    }

                    if (!isRunning) {
                        logger->warning("[MAIN] Mediator service did not start within timeout");
                        logger->warning("[MAIN] RF features will be unavailable");
                        logger->info("[MAIN] Check status: sudo systemctl status smarthome-mediator.service");
                    }
                    break;
                }
        }
    }

    // Run core main loop
    logger->debug("[MAIN] Running Core...");
    core.run();
    logger->debug("[MAIN] Core stopped running");


    // Wait for mediator to exit if enabled
    if (mediatorConfig.isEnabled && mediator) {
        logger->debug("[MAIN] Waiting for mediator shutdown");
        if (mediatorConfig.serviceType == Utils::ServiceType::STANDALONE) {
            mediator.terminate(); //SIGTERM
            if (!mediator.wait_for(15s)) {
                kill(mediator.id(), SIGKILL);
                mediator.wait();
            }
        } else if (mediatorConfig.serviceType == Utils::ServiceType::SYSTEMD) {
            logger->debug("[MAIN] Mediator in SYSTEMD mode - not stopping (managed by systemd)");
        }
    }

    logger->debug("[MAIN] Exiting");
    return 0;
}
