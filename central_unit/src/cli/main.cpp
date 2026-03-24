#include "main.h"

#include <iostream>
#include <string>

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

namespace si = SmartHome::IPC;
using namespace std::chrono_literals;

namespace SmartHomeCLI {
    bool attemptUdsConnection(si::SocketConnection &connection, const std::string &udsEndpointPath) {
        const auto endpoint = bal::stream_protocol::endpoint(udsEndpointPath);

        try {
            auto &socket = std::get<bal::stream_protocol::socket>(connection.mSocket);
            socket.connect(endpoint);
            return true;
        } catch (std::exception &e) {
            logger.debugf("[SMARTHOMECTL] UDS connection attempt failed: %s", e.what());
            return false;
        }
    }

    bool attemptTcpConnection(si::SocketConnection &connection,
                              const std::string &tcpEndpointAddress,
                              const int tcpEndpointPort) {
        const auto endpoint = bai::tcp::endpoint(bai::make_address(tcpEndpointAddress), tcpEndpointPort);

        try {
            auto &socket = std::get<bai::tcp::socket>(connection.mSocket);
            socket.connect(endpoint);
            return true;
        } catch (std::exception &e) {
            std::cerr << "TCP connection attempt error: " << e.what() << std::endl;
            logger.debugf("[SMARTHOMECTL] TCP connection attempt failed: %s", e.what());
            return false;
        }
    }

    void runCli(si::SocketConnection &connection) {
        std::this_thread::sleep_for(100ms); // Wait for logs
        std::string input;
        while (connection.isOpen()) {
            std::cout << "Smarthomectl: " << std::flush;

            if (!std::getline(std::cin, input)) {
                break; // break on read error
            }

            if (input.empty()) {
                continue; // do nothing on empty input
            }

            try {
                connection.write(std::move(input));
                std::string output = connection.read();
                std::cout << "\r\033[K" << output << std::endl;
            } catch (bs::system_error &e) {
                logger.errorf("[SMARTHOMECTL] IO error: %s", e.what());
            }
        }
    }

    void loggerShutdown() {
        logger.debug("[SMARTHOMECTL] Exiting");
        loggerGuard.reset();
        loggerThread->join();
    }
}


int main(const int argc, char *argv[]) {
    // Define program options
    bpo::options_description generic("Generic options");
    generic.add_options()
            ("help,h", "produce help message")

            ("uds,u",
             bpo::value<std::string>()->default_value(SmartHomeCLI::s_DEFAULT_UDS_ENDPOINT_PATH.data())->implicit_value(
                 SmartHomeCLI::s_DEFAULT_UDS_ENDPOINT_PATH.data()),
             "UDS endpoint socket file path")

            ("tcp,t", bpo::value<std::string>()->implicit_value(SmartHomeCLI::s_DEFAULT_TCP_ENDPOINT_ADDRESS.data()),
             "TCP ipv4 address and port in address:port format");

    //TODO consider implementing more advanced options
    //TODO add logger options and default values

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

    auto &ioContext = SmartHomeCLI::ioContext;

    //TODO Change to utility thread and add signal handling
    SmartHomeCLI::loggerGuard.emplace(ba::make_work_guard(ioContext));
    SmartHomeCLI::loggerThread = std::thread([&] {
        ioContext.run();
    });

    auto logger = std::make_shared<SmartHome::Utils::Logger>();
    logger->setLevel(su::LogLevels::Level::DEBUG); //TODO remove setting to debug level
    logger->info("[SMARTHOMECTL] Logger enabled");

    if (ipcType == si::SocketConnection::Type::UDS) {
        auto udsConnection = si::SocketConnection(ioContext, si::SocketConnection::Type::UDS, logger);

        std::string udsEndpointPath;
        try {
            udsEndpointPath = vm["uds"].as<std::string>();
        } catch (std::exception &e) {
            std::cerr << e.what() << std::endl;
            logger->criticalf("[SMARTHOMECTL] UDS connection unexpected error: %s", e.what());
            SmartHomeCLI::loggerShutdown();
            return EXIT_FAILURE;
        }

        if (SmartHomeCLI::attemptUdsConnection(udsConnection, udsEndpointPath)) {
            logger->info("[SMARTHOMECTL] Connected via UDS");
            SmartHomeCLI::runCli(udsConnection);

            udsConnection.close();
            SmartHomeCLI::loggerShutdown();
            return EXIT_SUCCESS;
        }
    } else {
        auto tcpConnection = si::SocketConnection(ioContext, si::SocketConnection::Type::TCP, logger);

        std::string tcpEndpointAddress;
        int tcpEndpointPort = 0;

        try {
            auto tcpEndpointAddressFull = vm["tcp"].as<std::string>();
            std::vector<std::string> tmp = {};
            boost::split(tmp, tcpEndpointAddressFull, boost::is_any_of(":"));
            if (tmp.size() == 2) {
                tcpEndpointAddress = tmp[0];
                tcpEndpointPort = std::stoi(tmp[1]);
            } else {
                logger->critical("[SMARTHOMECTL] TCP endpoint address is invalid");
                SmartHomeCLI::loggerShutdown();
                return EXIT_FAILURE;
            }
        } catch (std::exception &e) {
            std::cerr << e.what() << std::endl;
            logger->criticalf("[SMARTHOMECTL] TCP connection unexpected error: %s", e.what());
            SmartHomeCLI::loggerShutdown();
            return EXIT_FAILURE;
        }

        if (SmartHomeCLI::attemptTcpConnection(tcpConnection, tcpEndpointAddress, tcpEndpointPort)) {
            logger->info("[SMARTHOMECTL] Connected via TCP");
            SmartHomeCLI::runCli(tcpConnection);

            tcpConnection.close();
            logger->debug("[SMARTHOMECTL] Exiting");
            SmartHomeCLI::loggerShutdown();
            return EXIT_SUCCESS;
        }
    }

    logger->critical("[SMARTHOMECTL] Connection failure - smarthomed not reachable");
    SmartHomeCLI::loggerShutdown();
    return EXIT_FAILURE;
}
