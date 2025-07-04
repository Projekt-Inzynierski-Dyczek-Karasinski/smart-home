#include "smart_home_core.h"
#include "smart_home_config.h"

#include <iostream>

#include <boost/process.hpp>
#include <boost/program_options.hpp>

namespace bp = boost::process;
namespace bpo = boost::program_options;

//TODO implement logging system
int main(int argc, char *argv[]) {
    // Define program options
    bpo::options_description generic("Generic options");
    generic.add_options()
            ("help,h", "produce help message")
            ("port,p", bpo::value<unsigned short>(), "TCP IPC port")
            ("ipv4,4", bpo::value<std::string>(), "TCP IPC ipv4 address")
            ("config,c", bpo::value<std::string>(), "Path to main config file");

    //TODO add options
    //TODO add error handling for unknown options

    // Handle options
    bpo::variables_map vm;
    bpo::store(bpo::parse_command_line(argc, argv, generic), vm);
    bpo::notify(vm);

    // Print help in cmd on help option
    if (vm.contains("help")) {
        std::cout << generic << std::endl;
        return 0;
    }

    // Load smart home config
    const std::string configPath = vm.contains("config")
                                       ? vm["config"].as<std::string>()
                                       : "../../configs/smart_home.yaml";
    //TODO change path to global install

    auto &config = SmartHome::Utils::Config::Instance();
    if (config.loadConfig(configPath) == false) {
        std::cerr << "Failed to load smart home config" << std::endl;
        return 1;
    };

    // Enable mediator if set in config
    bp::child mediator;
    if (config.getValue<bool>("services.mediator.enabled").value_or(false))
        mediator = bp::child("/usr/bin/konsole", "-e", "./ModuleMediator");

    // Configure core
    auto &core = SmartHome::Core::Instance();
    SmartHome::Core::Config coreConfig; //Load default values

    //Overwrite default core config with smart home config
    config.getValue("core.ipc.tcp_server.enabled", coreConfig.tcpServerEnabled);
    config.getValue("core.ipc.tcp_server.address", coreConfig.tcpServerEndpointAddress);
    config.getValue("core.ipc.tcp_server.port", coreConfig.tcpServerEndpointPort);
    config.getValue("core.ipc.tcp_server.threads", coreConfig.tcpServerThreads);

    // Overwrite core config with cmd options
    if (vm.contains("port")) {
        coreConfig.tcpServerEndpointPort = vm["port"].as<int>();
    }

    if (vm.contains("ipv4")) {
        coreConfig.tcpServerEndpointAddress = vm["ipv4"].as<std::string>();
    }

    //Initialize Core
    if (core.initialize(coreConfig) == false) {
        std::cerr << "Failed to initialize core" << std::endl;
        return 1;
    }

    // Run core
    core.run();
    // Wait for mediator to exit if enabled
    if (mediator)
        mediator.wait();

    return 0;
}
