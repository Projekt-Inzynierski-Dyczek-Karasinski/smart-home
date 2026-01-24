#pragma once

#include "database_connection_manager.h"
#include "socket_server.h"
#include "service_manager/service_manager.h"
#include "async_logger.h"

#include <atomic>
#include <memory>
#include <optional>
#include <thread>
#include <utility>

#include <boost/asio.hpp>
#include <pqxx/pqxx>

namespace SmartHomeDB {
    namespace ba = boost::asio;
    namespace bs = boost::system;
    namespace si = SmartHome::IPC;


    class DatabaseService {


    public:
        /**
         * @brief Configuration structure for DatabaseService initialization.
         */
        struct Config {
            /// Default TCP config from socket server
            si::SocketServer::Config::Tcp tcp{
            .isEnabled = true, .endpointAddress = "127.0.0.1", .endpointPort = 43321};
            /// Default UDS config from socket server
            si::SocketServer::Config::Uds uds{
            .isEnabled = true, .endpointPath = "/var/run/smarthomed.sock"};

            DatabaseConnectionManager::Config dbConnConfig{};
        };
    };
}