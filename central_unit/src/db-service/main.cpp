#include "main.h"
#include "constants.h"

#include <iostream>
#include <string>

#include <boost/asio.hpp>
#include <boost/program_options.hpp>


namespace ba = boost::asio;
namespace bai = boost::asio::ip;
namespace bpo = boost::program_options;
namespace sc = SmartHome::Constants;

namespace SmartHomeDB {
    TerminalPasswordGuard::TerminalPasswordGuard() {
        tcgetattr(STDIN_FILENO, &mOldSettings);
        termios newSettings = mOldSettings;
        newSettings.c_lflag &= ~(ECHO | ECHOK | ECHONL);
        tcsetattr(STDIN_FILENO, TCSANOW, &newSettings);
    }

    TerminalPasswordGuard::~TerminalPasswordGuard() {
        tcsetattr(STDIN_FILENO, TCSANOW, &mOldSettings);
    }

    void loadLoggerYamlConfig(su::ConfigManager &configManager, su::Logger::Config &config) {
        std::string root = "db_service.logging";
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
        std::string root = "db_service";
        configManager.getValue(root + ".service_name", databaseServiceConfig.serviceName);
        configManager.getValue(root + ".service_name", databaseServiceConfig.dbConnConfig.serviceName);
        configManager.getValue(root + ".db_triggers_to_listen", databaseServiceConfig.dbTriggersToListen);

        root = "db_service.db_server";
        configManager.getValue(root + ".connections", databaseServiceConfig.dbConnConfig.dbConnections);
        configManager.getValue(root + ".host", databaseServiceConfig.dbConnConfig.dbHost);
        configManager.getValue(root + ".port", databaseServiceConfig.dbConnConfig.dbPort);
        configManager.getValue(root + ".name", databaseServiceConfig.dbConnConfig.dbName);
        configManager.getValue(root + ".user", databaseServiceConfig.dbConnConfig.dbUser);
        configManager.getValue(root + ".password", databaseServiceConfig.dbConnConfig.dbPassword);
        configManager.getValue(root + ".connection_timeout_seconds",
                               databaseServiceConfig.dbConnConfig.connectionTimeoutSeconds);
        configManager.getValue(root + ".keep_alive_enabled", databaseServiceConfig.dbConnConfig.isKeepAliveEnabled);
        configManager.getValue(root + ".keep_alive_interval_seconds",
                               databaseServiceConfig.dbConnConfig.keepAliveSeconds);

        root = "db_service.ipc.tcp";
        configManager.getValue(root + ".enabled", databaseServiceConfig.tcp.isEnabled);
        configManager.getValue(root + ".address", databaseServiceConfig.tcp.endpointAddress);
        configManager.getValue(root + ".port", databaseServiceConfig.tcp.endpointPort);

        root = "db_service.ipc.uds";
        configManager.getValue(root + ".enabled", databaseServiceConfig.uds.isEnabled);
        configManager.getValue(root + ".socket_path", databaseServiceConfig.uds.endpointPath);
    }

    void overWriteConfigsWithEnvironmentVariables(DatabaseService::Config &databaseServiceConfig,
                                                  const std::shared_ptr<su::Logger> &pLogger) {
        if (const char *envVar = std::getenv("SH_DB_HOST")) databaseServiceConfig.dbConnConfig.dbHost = envVar;
        if (const char *envVar = std::getenv("SH_DB_PORT")) {
            try {
                const uint numericValue = std::stoi(envVar);
                databaseServiceConfig.dbConnConfig.dbPort = numericValue;
            } catch (std::exception &e) {
                pLogger->errorf("[MAIN_DB-SERVICE] Failed to parse port from environment variable: %s", e.what());
            }
        }
        if (const char *envVar = std::getenv("SH_DB_NAME")) databaseServiceConfig.dbConnConfig.dbName = envVar;
        if (const char *envVar = std::getenv("SH_DB_USER")) databaseServiceConfig.dbConnConfig.dbUser = envVar;
        if (const char *envVar = std::getenv("SH_DB_PASSWORD")) databaseServiceConfig.dbConnConfig.dbPassword = envVar;
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

        if (vm.contains("db-host")) databaseServiceConfig.dbConnConfig.dbHost = vm["db-host"].as<std::string>();
        if (vm.contains("db-port")) databaseServiceConfig.dbConnConfig.dbPort = vm["db-port"].as<int>();
        if (vm.contains("db-name")) databaseServiceConfig.dbConnConfig.dbName = vm["db-name"].as<std::string>();
        if (vm.contains("db-user")) databaseServiceConfig.dbConnConfig.dbUser = vm["db-user"].as<std::string>();
    }

    void loadConfigs(const bpo::variables_map &vm,
                     const bpo::parsed_options &parsed,
                     const std::shared_ptr<su::Logger> &pLogger,
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
            pLogger->disableConsoleLogging();
            logTmpOpt.isQuietSet = true;
        }

        if (vm.contains("log-level")) {
            // Overwrite verbose with explicit log-level
            logTmpOpt.verboseLevel = vm["log-level"].as<uint8_t>();
            logTmpOpt.isVerbositySet = true;
        }

        if (logTmpOpt.isVerbositySet) pLogger->setLevel(su::LogLevels::toLevel(logTmpOpt.verboseLevel));

        // Load YAML config
        auto configManager = su::ConfigManager(pLogger);
        su::Logger::Config loggerConfig;
        loggerConfig.logFile.path = sc::DefaultPaths::DB_SERVICE_LOGFILE;
        const std::string configPath = vm.contains("config")
                                           ? vm["config"].as<std::string>()
                                           : sc::DefaultPaths::DB_SERVICE_CONFIG.data();
        if (configManager.loadConfig(configPath)) {
            pLogger->debug("[MAIN_DB-SERVICE] Loading YAML logger config");
            loadLoggerYamlConfig(configManager, loggerConfig);

            pLogger->debug("[MAIN_DB-SERVICE] Loading YAML core config");
            loadYamlConfigs(configManager, databaseServiceConfig);
        } else {
            pLogger->warning("[MAIN_DB-SERVICE] Could not load YAML config");
        }

        overWriteConfigsWithEnvironmentVariables(databaseServiceConfig, pLogger);

        overwriteConfigsWithProgramOptions(vm, logTmpOpt, databaseServiceConfig, loggerConfig);

        pLogger->applyConfig(loggerConfig);
        pLogger->debug("[MAIN_DB-SERVICE] Smarthome database service configured");
    }

    std::string getPassword() {
        TerminalPasswordGuard guard;
        std::string password;
        std::getline(std::cin, password);
        std::cout << std::endl;
        return password;
    }
}

using namespace SmartHomeDB;

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

    bpo::options_description network("Network IPC options");
    network.add_options()
            ("port,p", bpo::value<int>(), "TCP IPC port")
            ("ipv4,a", bpo::value<std::string>(), "TCP IPC ipv4 address");

    bpo::options_description database("Database options");
    database.add_options()
            ("db-host,H", bpo::value<std::string>(), "DB Host name / address")
            ("db-port,P", bpo::value<int>(), "DB Host port")
            ("db-name,D", bpo::value<std::string>(), "DB name, program will prompt for password")
            ("db-user,U", bpo::value<std::string>(), "DB user name");


    bpo::options_description allOptions;
    allOptions.add(generic).add(logging).add(network).add(database);

    bpo::options_description helpFormat("");
    helpFormat.add(generic).add(logging).add(network).add(database);

    bpo::options_description errorFormat("Allowed options:");
    errorFormat.add_options();
    for (const auto &opt: generic.options()) errorFormat.add(opt);
    for (const auto &opt: logging.options()) errorFormat.add(opt);
    for (const auto &opt: network.options()) errorFormat.add(opt);
    for (const auto &opt: database.options()) errorFormat.add(opt);

    // Handle options
    bpo::variables_map vm;
    std::unique_ptr<bpo::parsed_options> pParsed;
    try {
        pParsed = std::make_unique<bpo::parsed_options>(bpo::command_line_parser(argc, argv).options(allOptions).run());
        bpo::store(*pParsed, vm);
        bpo::notify(vm);
    } catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
        std::cerr << "Usage:\n  smarthome-database [options]\n";
        std::cout << errorFormat << std::endl;
        return EXIT_FAILURE;
    }
    // Print help in cmd on help option
    if (vm.contains("help")) {
        std::cerr << "Usage:\n  smarthome-database [options]\n";
        std::cout << helpFormat << std::endl;
        return 0;
    }

    std::optional<std::string> terminalInputPassword; // string with password - overwrites config and env value if set
    if (vm.contains("db-user")) {
        std::cout << "Trying to log in as database user '" << vm["db-user"].as<std::string>() << "'" << std::endl;
        std::cout << "Enter password: ";
        terminalInputPassword = getPassword();
    }

    // Define instances
    auto pLogger = std::make_shared<su::Logger>();
    auto &dbService = DatabaseService::Instance();

    DatabaseService::Config dbConfig;

    loadConfigs(vm, *pParsed, pLogger, dbConfig);

    // Overwrite password if set from terminal
    if (terminalInputPassword.has_value()) {
        dbConfig.dbConnConfig.dbPassword = terminalInputPassword.value();
    }

    if (!dbService.initialize(dbConfig, pLogger)) {
        pLogger->critical("[MAIN_DB-SERVICE] Failed to initialize database service");
        return EXIT_FAILURE;
    }

    dbService.run();

    pLogger->info("[MAIN_DB-SERVICE] Exit");
    return EXIT_SUCCESS;
}
