#pragma once

#include "socket_connection.h"

#include <utility>

#include <boost/asio.hpp>
#include <boost/program_options.hpp>


namespace ba = boost::asio;
namespace bai = boost::asio::ip;
namespace bpo = boost::program_options;

namespace SmartHomeCLI {
    /**
     * @brief Tries connecting with IPC server UDS endpoint.
     *
     * @param connection A connection instance reference.
     * @param udsEndpointPath Socket file for Unix domain socket connection.
     * @return true if successfully, false otherwise.
     *
     */
    bool attemptUdsConnection(SmartHome::IPC::SocketConnection &connection, const std::string &udsEndpointPath);

    /**
     * @brief Tries connecting with IPC server TCP endpoint.
     *
     * @param connection A connection instance reference.
     * @param tcpEndpointAddress IP address for TCP connection.
     * @param tcpEndpointPort Port for TCP connection.
     * @return true if successfully, false otherwise.
     */
    bool attemptTcpConnection(SmartHome::IPC::SocketConnection &connection,
                              const std::string &tcpEndpointAddress,
                              int tcpEndpointPort);

    /**
     * @brief Begins write/read loop, sending stdin to server and printing response to stdout.
     *
     * @param connection A connection instance reference.
     */
    void runCli(SmartHome::IPC::SocketConnection &connection);

    /// Default IP address for TCP connection (address:port)
    static constexpr const char *s_DEFAULT_TCP_ENDPOINT_ADDRESS = "127.0.0.1:43321";
    /// Default path for Unix Domain Socket file
    static constexpr const char *s_DEFAULT_UDS_ENDPOINT_PATH = "/var/run/smarthomed.sock";
}
