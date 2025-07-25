#include "main.h"
#include "core.h"
#include "config_manager.h"

#include <iostream>
#include <filesystem>

#include <boost/process.hpp>
#include <boost/program_options.hpp>

namespace bp = boost::process;
namespace bpo = boost::program_options;

namespace SmartHome {
    //TODO implement logging system
    void loadConfigValues(Utils::ConfigManager &configManager, Core::Config *coreConfig,
                          MediatorConfig *mediatorConfig) {
        // Mediator config
        std::string root = "services.mediator";
        configManager.getValue(root + ".enabled", mediatorConfig->isEnabled);
        mediatorConfig->serviceType = Utils::resolveServiceType(
            configManager.getValue<std::string>(root + ".service_type").value());
        configManager.getValue(root + ".exec_path", mediatorConfig->execPath);
        configManager.getValue(root + ".config_path", mediatorConfig->configPath);

        // Core config
        root = "core.ipc";
        configManager.getValue(root + ".threads", coreConfig->ipcServerThreads);

        root = "core.ipc.tcp";
        configManager.getValue(root + ".enabled", coreConfig->tcp.isEnabled);
        configManager.getValue(root + ".address", coreConfig->tcp.endpointAddress);
        configManager.getValue(root + ".port", coreConfig->tcp.endpointPort);

        root = "core.ipc.uds";
        configManager.getValue(root + ".enabled", coreConfig->uds.isEnabled);
        configManager.getValue(root + ".socket_path", coreConfig->uds.endpointPath);
    }
}

using namespace SmartHome;

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

    // Load default values
    auto &core = Core::Instance();
    Core::Config coreConfig;

    bp::child mediator;
    MediatorConfig mediatorConfig;

    // Initialize YAML config manager
    auto &configManager = Utils::ConfigManager::Instance();
    const std::string configPath = vm.contains("config") ? vm["config"].as<std::string>() : sDefaultConfigPath;

    //Load YAML config values
    if (configManager.loadConfig(configPath)) {
        loadConfigValues(configManager, &coreConfig, &mediatorConfig);
    } else {
        std::cerr << "Could not load YAML config" << std::endl;
    }

    // Overwrite YAML config with cmd options
    if (vm.contains("port")) coreConfig.tcp.endpointPort = vm["port"].as<int>();
    if (vm.contains("ipv4")) coreConfig.tcp.endpointAddress = vm["ipv4"].as<std::string>();


    //Initialize Core
    std::cout << coreConfig.uds.endpointPath << std::endl;
    if (!core.initialize(coreConfig)) {
        std::cerr << "Failed to initialize core" << std::endl;
        return 1;
    }

    // Launch mediator if enabled
    if (mediatorConfig.isEnabled) {
        switch (mediatorConfig.serviceType) {
            default:
            case Utils::ServiceType::AUTO:
            case Utils::ServiceType::STANDALONE:
                if (std::filesystem::exists(mediatorConfig.execPath)) {
                    mediator = bp::child(mediatorConfig.execPath);
                } else {
                    std::cerr << "Could not find mediator executable" << std::endl;
                }
                break;
            case Utils::ServiceType::SYSTEMD:
                //TODO implement
                break;
        }
    }

    // Run core
    core.run();

    // Wait for mediator to exit if enabled
    if (mediatorConfig.isEnabled && mediator) {
        if (mediatorConfig.serviceType == Utils::ServiceType::STANDALONE) {
            mediator.terminate(); //SIGTERM
            if (!mediator.wait_for(std::chrono::seconds(15))) {
                mediator.terminate(); // SIGKILL
            }
        } else if (mediatorConfig.serviceType == Utils::ServiceType::SYSTEMD) {
            //TODO implement
        }
    }

    return 0;
}
