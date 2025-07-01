#include "smart_home_core.h"

#include <iostream>
#include <boost/process.hpp>
#include <boost/program_options.hpp>

namespace bp = boost::process;
namespace bpo = boost::program_options;

int main(int argc, char *argv[]) {
    // Define program options
    bpo::options_description generic("Generic options");
    generic.add_options()
            ("help,h", "produce help message")
            ("port,p", bpo::value<int>()->default_value(43321), "TCP port");

    //TODO add options
    //TODO add error handling for unknown options

    bpo::variables_map vm;
    bpo::store(bpo::parse_command_line(argc, argv, generic), vm);
    bpo::notify(vm);

    if (vm.contains("help")) {
        std::cout << generic << std::endl;
        return 0;
    }

    int port = vm["port"].as<int>();
    // std::cout << port << std::endl;

    auto &core = SmartHome::Core::Instance();
    //TODO pass port to Core
    //TODO implement logging system

    //TODO read from config if mediator should be run
    bp::child mediator("/usr/bin/konsole", "-e", "./ModuleMediator");

    if (core.initialize() == false) {
        std::cerr << "Failed to initialize core" << std::endl;
        return 1;
    }

    core.run();
    mediator.wait();

    return 0;
}
