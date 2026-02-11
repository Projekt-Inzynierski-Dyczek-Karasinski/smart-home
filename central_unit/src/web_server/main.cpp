#include "main.h"
#include "constants.h"

#include <iostream>
#include <string>

namespace SmartHomeWebServer {
    namespace sc = SmartHome::Constants;

    void loadLoggerYamlConfig(su::ConfigManager &configManager, su::Logger::Config &config) {
        std::string root = "web_server.logging";
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

    void loadYamlConfigs(su::ConfigManager &configManager, WebServer::Config &webServerConfig) {
        std::string root = "web_server";
        configManager.getValue(root + ".service_name", webServerConfig.serviceName);

        root = "web_server.http";
        configManager.getValue(root + ".port", webServerConfig.http.port);
        configManager.getValue(root + ".static_root", webServerConfig.http.staticRoot);

        root = "web_server.cors";
        configManager.getValue(root + ".allow_origin", webServerConfig.cors.allowOrigin);

        root = "web_server.ipc.tcp";
        configManager.getValue(root + ".enabled", webServerConfig.apiClient.tcp.isEnabled);
        configManager.getValue(root + ".address", webServerConfig.apiClient.tcp.endpointAddress);
        configManager.getValue(root + ".port", webServerConfig.apiClient.tcp.endpointPort);

        root = "web_server.ipc.uds";
        configManager.getValue(root + ".enabled", webServerConfig.apiClient.uds.isEnabled);
        configManager.getValue(root + ".socket_path", webServerConfig.apiClient.uds.endpointPath);
    }

    void overwriteConfigsWithProgramOptions(const bpo::variables_map &vm,
                                            const LoggerTemporaryOptions &logTmpOpt,
                                            WebServer::Config &webServerConfig,
                                            su::Logger::Config &loggerConfig) {
        // Logger config
        if (logTmpOpt.isVerbositySet) loggerConfig.logLevel = su::LogLevels::toLevel(logTmpOpt.verboseLevel);
        if (logTmpOpt.isQuietSet) loggerConfig.enableConsoleLogOutput = false;
        if (vm.contains("no-log-file")) loggerConfig.logFile.enabled = false;
        if (vm.contains("log-file")) loggerConfig.logFile.path = vm["log-file"].as<std::string>();
        loggerConfig.logFile.createNew = !vm["append-log"].as<bool>();
        loggerConfig.logFile.archiveOld = !vm["no-archive"].as<bool>();

        // WebServer config
        if (vm.contains("http-port")) webServerConfig.http.port = vm["http-port"].as<int>();
        if (vm.contains("static-root")) webServerConfig.http.staticRoot = vm["static-root"].as<std::string>();
        if (vm.contains("port")) webServerConfig.apiClient.tcp.endpointPort = vm["port"].as<int>();
        if (vm.contains("ipv4")) webServerConfig.apiClient.tcp.endpointAddress = vm["ipv4"].as<std::string>();
    }

    void loadConfigs(const bpo::variables_map &vm,
                     const bpo::parsed_options &parsed,
                     const std::shared_ptr<su::Logger> &pLogger,
                     WebServer::Config &webServerConfig) {
        LoggerTemporaryOptions logTmpOpt;

        // Set temporary logger config based on program options
        if (vm.contains("verbose")) {
            for (const auto &option: parsed.options) {
                if (option.string_key == "verbose") {
                    logTmpOpt.verboseLevel++;
                }
            }
            logTmpOpt.isVerbositySet = true;
        }

        if (vm.contains("quiet")) {
            pLogger->disableConsoleLogging();
            logTmpOpt.isQuietSet = true;
        }

        if (vm.contains("log-level")) {
            logTmpOpt.verboseLevel = vm["log-level"].as<uint8_t>();
            logTmpOpt.isVerbositySet = true;
        }

        if (logTmpOpt.isVerbositySet) pLogger->setLevel(su::LogLevels::toLevel(logTmpOpt.verboseLevel));

        // Load YAML config
        auto configManager = su::ConfigManager(pLogger);
        su::Logger::Config loggerConfig;
        loggerConfig.logFile.path = sc::DefaultPaths::WEB_SERVER_LOGFILE;

        const std::string configPath = vm.contains("config")
                                           ? vm["config"].as<std::string>()
                                           : sc::DefaultPaths::WEB_SERVER_CONFIG.data();
        if (configManager.loadConfig(configPath)) {
            pLogger->debug("[MAIN_WEB-SERVER] Loading YAML logger config");
            loadLoggerYamlConfig(configManager, loggerConfig);

            pLogger->debug("[MAIN_WEB-SERVER] Loading YAML web server config");
            loadYamlConfigs(configManager, webServerConfig);
        } else {
            pLogger->warning("[MAIN_WEB-SERVER] Could not load YAML config, using defaults");
        }

        overwriteConfigsWithProgramOptions(vm, logTmpOpt, webServerConfig, loggerConfig);

        pLogger->applyConfig(loggerConfig);
        pLogger->debug("[MAIN_WEB-SERVER] Web server configured");
    }
}

using namespace SmartHomeWebServer;

int main(int argc, char *argv[]) {
    // Define program options
    bpo::options_description generic("Generic options");
    generic.add_options()
            ("help,h", "Produce help message")
            ("config,c", bpo::value<std::string>(), "Path to config file");

    bpo::options_description logging("Logging options");
    logging.add_options()
            ("verbose,v", bpo::value<int>()->implicit_value(0)->zero_tokens()->composing(),
             "Increase verbosity/log-level (-v=WARNING,-vv=INFO,-vvv=DEBUG)")
            ("quiet,q", "Suppress console output")
            ("log-level,l", bpo::value<uint8_t>(),
             "Set verbosity/log-level (0=NONE, 1=CRITICAL 2=ERROR, 3=WARNING, 4=INFO, 5=DEBUG)")
            ("log-file", bpo::value<std::string>(), "Log file path")
            ("append-log", bpo::bool_switch()->default_value(false), "Append to existing log file")
            ("no-archive", bpo::bool_switch()->default_value(false), "Disables archiving old log files")
            ("no-log-file", "Disable file logging");

    bpo::options_description network("Network options");
    network.add_options()
            ("port,p", bpo::value<int>(), "TCP IPC port (connection to core)")
            ("ipv4,4", bpo::value<std::string>(), "TCP IPC ipv4 address (connection to core)");

    bpo::options_description http("HTTP server options");
    http.add_options()
            ("http-port", bpo::value<int>(), "HTTP server port (default: 8080)")
            ("static-root", bpo::value<std::string>(), "Path to static web files directory");

    bpo::options_description allOptions;
    allOptions.add(generic).add(logging).add(network).add(http);

    bpo::options_description helpFormat("");
    helpFormat.add(generic).add(logging).add(network).add(http);

    bpo::options_description errorFormat("Allowed options:");
    for (const auto &opt: generic.options()) errorFormat.add(opt);
    for (const auto &opt: logging.options()) errorFormat.add(opt);
    for (const auto &opt: network.options()) errorFormat.add(opt);
    for (const auto &opt: http.options()) errorFormat.add(opt);

    // Parse options
    bpo::variables_map vm;
    std::unique_ptr<bpo::parsed_options> pParsed;
    try {
        pParsed = std::make_unique<bpo::parsed_options>(
            bpo::command_line_parser(argc, argv).options(allOptions).run());
        bpo::store(*pParsed, vm);
        bpo::notify(vm);
    } catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
        std::cerr << "Usage:\n  smarthome-web-server [options]\n";
        std::cout << errorFormat << std::endl;
        return EXIT_FAILURE;
    }

    if (vm.contains("help")) {
        std::cerr << "Usage:\n  smarthome-web-server [options]\n";
        std::cout << helpFormat << std::endl;
        return EXIT_SUCCESS;
    }

    // Initialize
    auto pLogger = std::make_shared<su::Logger>();
    auto &webServer = WebServer::Instance();

    WebServer::Config config;
    loadConfigs(vm, *pParsed, pLogger, config);

    if (!webServer.initialize(config, pLogger)) {
        pLogger->critical("[MAIN_WEB-SERVER] Failed to initialize web server");
        return EXIT_FAILURE;
    }

    webServer.run();

    pLogger->info("[MAIN_WEB-SERVER] Exit");
    return EXIT_SUCCESS;
}
