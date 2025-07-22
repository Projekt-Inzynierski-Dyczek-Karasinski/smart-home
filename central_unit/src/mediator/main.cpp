#include <iostream>
#include <string>
#include <utility>

#include <boost/asio.hpp>
#include <boost/program_options.hpp>

namespace ba = boost::asio;
namespace bai = boost::asio::ip;
namespace bpo = boost::program_options;

// TODO this is placeholder version for testing implement working mediator
int main(int argc, char *argv[]) {
    // Define program options
    bpo::options_description generic("Generic options");
    generic.add_options()
            ("help,h", "produce help message")
            ("port,p", bpo::value<int>()->default_value(43321), "TCP port")
            ("ipv4,4", bpo::value<std::string>()->default_value("localhost"), "TCP ipv4 address");

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
    std::string ip = vm["ipv4"].as<std::string>();


    // Establish connection
    std::cout << "Trying connecting with: " << ip << ":" << port << std::endl;

    ba::io_context ioContext;
    bai::tcp::resolver resolver(ioContext);
    bai::tcp::socket socket(ioContext);

    try {
        ba::connect(socket, resolver.resolve(ip, std::to_string(port)));
    } catch (std::exception &e) {
        std::cerr << "Connection error: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "Connected to server" << std::endl;

    // Main loop for testing
    std::string message;
    std::cout << "Enter messages (exit to quit):" << std::endl;

    while (std::getline(std::cin, message)) {
        if (message == "exit") break;

        message += "\n";
        ba::write(socket, ba::buffer(message));
    }

    socket.close();

    return 0;
}
