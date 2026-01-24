#pragma once
#include "logger.h"

#include <condition_variable>
#include <memory>
#include <string>
#include <queue>
#include <pqxx/connection.hxx>

namespace SmartHomeDB {
    class DatabaseConnection;

    class DatabaseConnectionManager {
    public:
        struct Config {
            uint dbMaxConnections = 1;

            // DB connection target
            std::string dbHost = "127.0.0.1";
            uint dbPort = 5432;

            // DB credentials
            std::string dbName = "smart_home";
            std::string dbUser = "smart_home_admin";
            std::string dbPassword = ""; // No default password

            // Additional connection options
            std::string serviceName = "smarthome-database";
            uint connectionTimeoutSeconds = 10;
            bool isKeepAliveEnabled = true;
            uint keepAliveSeconds = 30;
        };

        /**
         *
         * @param pLogger
         * @param config
         *
         * @throws std::runtime_error when no connections are initialized.
         */
        explicit DatabaseConnectionManager(const std::shared_ptr<SmartHome::Utils::Logger> &pLogger,
                                           const Config &config);

        DatabaseConnection acquireConnection();

    private:
        friend class DatabaseConnection;

        void returnConnection(std::unique_ptr<pqxx::connection> connection);

        std::shared_ptr<SmartHome::Utils::Logger> mpLogger;

        std::string mConnectionStr;
        std::queue<std::unique_ptr<pqxx::connection> > mConnections;
        std::mutex mConnectionsMutex;
        std::condition_variable mConnectionsCond;
    };
}
