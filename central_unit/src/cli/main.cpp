#include "main.h"

#include <iostream>
#include <string>

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

namespace si = SmartHome::IPC;

namespace SmartHomeCLI {
    bool attemptUdsConnection(si::SocketConnection &connection, const std::string &udsEndpointPath) {
        const auto endpoint = bal::stream_protocol::endpoint(udsEndpointPath);

        try {
            auto &socket = std::get<bal::stream_protocol::socket>(connection.mSocket);
            socket.connect(endpoint);
            return true;
        } catch (std::exception &e) {
            std::cerr << "UDS connection attempt error:" << e.what() << std::endl;
            return false;
        }
    }

    bool attemptTcpConnection(si::SocketConnection &connection, const std::string &tcpEndpointAddress,
                              const int tcpEndpointPort) {
        const auto endpoint = bai::tcp::endpoint(bai::make_address(tcpEndpointAddress), tcpEndpointPort);

        try {
            auto &socket = std::get<bai::tcp::socket>(connection.mSocket);
            socket.connect(endpoint);
            return true;
        } catch (std::exception &e) {
            std::cerr<< "TCP connection attempt error: " << e.what() << std::endl;
            return false;
        }
    }

    void runCli(si::SocketConnection &connection) {
        std::string input = "";
        while (connection.isOpen()) {
            std::cout << "Smarthomectl: " << std::flush;

            if (!std::getline(std::cin, input)) {
                break; // break on read error
            }

            if (input.empty()) {
                continue; // do nothing on empty input
            }

            try {
                connection.write(input);
                std::string output = connection.read();
                std::cout << "\r\033[K" << output << std::endl;
            } catch (bs::system_error &e) {
                std::cout << e.what() << std::endl;
            }
        }
    }
}


int main(const int argc, char *argv[]) {
    // Define program options
    bpo::options_description generic("Generic options");
    generic.add_options()
            ("help,h", "produce help message")

            ("uds,u",
             bpo::value<std::string>()->default_value(SmartHomeCLI::DEFAULT_UDS_ENDPOINT_PATH)->implicit_value(
                 SmartHomeCLI::DEFAULT_UDS_ENDPOINT_PATH),
             "UDS endpoint socket file path")

            ("tcp,t", bpo::value<std::string>()->implicit_value(SmartHomeCLI::DEFAULT_TCP_ENDPOINT_ADDRESS),
             "TCP ipv4 address and port in address:port format");

    //TODO consider implementing more advanced options

    bpo::variables_map vm;
    try {
        bpo::store(bpo::parse_command_line(argc, argv, generic), vm);
    } catch (bpo::error &e) {
        std::cerr << e.what() << std::endl;
        std::cout << generic << std::endl;
        return EXIT_FAILURE;
    }
    bpo::notify(vm);

    if (vm.contains("help")) {
        std::cout << generic << std::endl;
        return EXIT_SUCCESS;
    }


    const si::SocketConnection::Type ipcType = vm.contains("tcp")
                                                   ? si::SocketConnection::Type::TCP
                                                   : si::SocketConnection::Type::UDS;

    ba::io_context ioContext;

    if (ipcType == si::SocketConnection::Type::UDS) {
        auto udsConnection = si::SocketConnection(ioContext, si::SocketConnection::Type::UDS);

        std::string udsEndpointPath;
        try {
            udsEndpointPath = vm["uds"].as<std::string>();
        } catch (std::exception &e) {
            std::cerr << e.what() << std::endl;
            return EXIT_FAILURE;
        }

        if (SmartHomeCLI::attemptUdsConnection(udsConnection, udsEndpointPath)) {
            std::cout << "Connected via UDS" << std::endl;
            SmartHomeCLI::runCli(udsConnection);

            udsConnection.close();
            return EXIT_SUCCESS;
        }
    } else {
        auto tcpConnection = si::SocketConnection(ioContext, si::SocketConnection::Type::TCP);

        std::string tcpEndpointAddress = "";
        int tcpEndpointPort = 0;

        try {
            std::string tcpEndpointAddressFull = vm["tcp"].as<std::string>();
            std::vector<std::string> tmp = {};
            boost::split(tmp, tcpEndpointAddressFull, boost::is_any_of(":"));
            if (tmp.size() == 2) {
                tcpEndpointAddress = tmp[0];
                tcpEndpointPort = std::stoi(tmp[1]);
            } else {
                std::cerr << "tcp endpoint address is invalid" << std::endl;
                return EXIT_FAILURE;
            }
        } catch (std::exception &e) {
            std::cerr << e.what() << std::endl;
            return EXIT_FAILURE;
        }

        if (SmartHomeCLI::attemptTcpConnection(tcpConnection, tcpEndpointAddress, tcpEndpointPort)) {
            std::cout << "Connected via TCP" << std::endl;
            SmartHomeCLI::runCli(tcpConnection);

            tcpConnection.close();
            return EXIT_SUCCESS;
        }
    }

    std::cerr << "Connection failure: smarthomed not reachable" << std::endl;
    return EXIT_FAILURE;
}
