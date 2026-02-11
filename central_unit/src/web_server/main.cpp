#include "main.h"
#include "constants.h"
#include "routes/core_routes.h"
#include "routes/sensor_routes.h"
#include "routes/actuator_routes.h"
#include "routes/module_routes.h"
#include "routes/database_routes.h"
#include "routes/static_routes.h"

#include <iostream>
#include <string>
#include <utility>
#include <thread>

#include <crow/middlewares/cors.h>
#include <crow.h>


using namespace SmartHomeWebServer;

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
        return EXIT_SUCCESS;
    }

    Config config;

    auto pLogger = std::make_shared<su::Logger>();

    pLogger->setLevel(SmartHome::Utils::LogLevels::Level::DEBUG); //TODO !pr set log level from options


    pLogger->debug("DEBUG");

    ba::io_context ioContext;

    auto pAsyncLogger = std::make_shared<su::AsyncLogger>(pLogger, ioContext);

    pAsyncLogger->setLevel(SmartHome::Utils::LogLevels::Level::DEBUG);

    ApiClient apiClient(pAsyncLogger, ioContext);
    config.apiClientConfig.tcp.endpointAddress = "192.168.100.33";
    apiClient.initialize(config.apiClientConfig);

    crow::App<crow::CORSHandler> app;

    auto &cors = app.get_middleware<crow::CORSHandler>();

    // TODO Allow all for development, configure properly for production
    cors.global()
            .origin("*")
            .methods("GET"_method, "POST"_method, "PUT"_method, "DELETE"_method, "OPTIONS"_method)
            .headers("*");


    std::string webRoot = "/home/duk/CLionProjects/smart-home/central_unit/src/web_server/static"; //TODO !pr
    registerCoreRoutes(app, apiClient);
    registerSensorRoutes(app, apiClient);
    registerActuatorRoutes(app, apiClient);
    registerModuleRoutes(app, apiClient);
    registerDatabaseRoutes(app, apiClient);
    registerStaticRoutes(app, webRoot);


    auto threadGuard = ba::make_work_guard(ioContext);
    std::thread thread([&] { ioContext.run(); });

    app.port(8080).multithreaded().run();

    threadGuard.reset();
    ioContext.stop();
    thread.join();

    return EXIT_SUCCESS;
}
