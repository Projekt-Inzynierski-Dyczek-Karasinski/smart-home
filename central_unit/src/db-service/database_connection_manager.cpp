#include "database_connection_manager.h"
#include "database_connection.h"

#include <mutex>


namespace SmartHomeDB {
    DatabaseConnectionManager::DatabaseConnectionManager(const std::shared_ptr<SmartHome::Utils::Logger> &pLogger,
                                                         const Config &config) : mpLogger(pLogger) {
        char buffer[2048];

        snprintf(buffer,
                 sizeof (buffer),
                 "host=%s "
                 "port=%d "
                 "dbname=%s "
                 "user=%s "
                 "password=%s "
                 "application_name=%s "
                 "connect_timeout=%d "
                 "keepalives=%d "
                 "keepalives_idle=%d",
                 config.dbHost.c_str(),
                 config.dbPort,
                 config.dbName.c_str(),
                 config.dbUser.c_str(),
                 config.dbPassword.c_str(),
                 config.serviceName.c_str(),
                 config.connectionTimeoutSeconds,
                 config.isKeepAliveEnabled ? 1 : 0,
                 config.keepAliveSeconds
        );

        mConnectionStr = std::string(buffer);

        mpLogger->debugf("[DB_CONN_MANAGER] [CONSTRUCTOR] Attempting to connect with: %s", config.dbHost.c_str());

        for (auto i = 0; i < config.dbConnections; ++i) {
            try {
                mConnections.push(std::make_unique<pqxx::connection>(mConnectionStr));
            } catch (const std::exception &e) {
                mpLogger->errorf("[DB_CONN_MANAGER] [CONSTRUCTOR] Failed to initialize connection: %s", e.what());
            }
        }

        if (mConnections.empty()) {
            throw std::runtime_error("No connections available");
        }
        mpLogger->infof("[DB_CONN_MANAGER] [CONSTRUCTOR] Successfully initialized %d of %d connections",
                        mConnections.size(), config.dbConnections);
    }

    DatabaseConnection DatabaseConnectionManager::acquireConnection() {
        std::unique_lock lock(mConnectionsMutex);

        // Await for connection if none are available in pool
        mConnectionsCond.wait(lock, [this]() { return !mConnections.empty(); });

        auto connection = std::move(mConnections.front());
        mConnections.pop();

        return {*this, std::move(connection)};
    }

    void DatabaseConnectionManager::returnConnection(std::unique_ptr<pqxx::connection> connection) {
        std::scoped_lock lock(mConnectionsMutex);

        if (connection->is_open()) {
            mConnections.push(std::move(connection));
        } else {
            try {
                mConnections.push(std::make_unique<pqxx::connection>(mConnectionStr));
            } catch (std::exception &e) {
                mpLogger->errorf("[DB_CONN_MANAGER] [RETURN_CONN] Failed to re-initialize connection: %s", e.what());
            }
        }

        mConnectionsCond.notify_one();
    }
}
