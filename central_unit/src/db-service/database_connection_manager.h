#pragma once
#include "logger.h"

#include <condition_variable>
#include <memory>
#include <string>
#include <queue>
#include <pqxx/pqxx>

namespace SmartHomeDB {
    class DatabaseConnection;

    /**
     * @brief Manager of a pool of PostgreSQL connections.
     *
     * @details Initializes a pool of `pqxx::connection` objects using the provided configuration.
     *          Provides RAII-style acquisition via `DatabaseConnection`
     *          and returns connections to the pool when they are released.
     */
    class DatabaseConnectionManager {
    public:
        /**
         * @brief Configuration for DatabaseConnectionManager.
         *
         * @details Holds connection count, target host/port, credentials and additional connection options.
         */
        struct Config {
            uint dbConnections = 1;

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
         * @brief Construct the connection manager and initialize the pool.
         *
         * @details Builds the connection string from \p config and attempts to create up
         *          to \c config.dbConnections connections which are stored in an internal
         *          queue for later acquisition.
         *
         * @param pLogger Shared logger used by the manager for diagnostics and errors.
         * @param config Configuration describing connection parameters and pool size.
         *
         * @throws std::runtime_error when no connections could be initialized.
         */
        explicit DatabaseConnectionManager(const std::shared_ptr<SmartHome::Utils::Logger> &pLogger,
                                           const Config &config);

        /**
         * @brief Acquire a connection from the pool.
         *
         * @details Blocks until a connection becomes available, then removes it from the internal queue and
         *          returns a \c DatabaseConnection that will return the connection to the pool on destruction.
         *
         * @return A \c DatabaseConnection owning the acquired \c pqxx::connection.
         */
        DatabaseConnection acquireConnection();

    private:
        friend class DatabaseConnection;

        /**
         * @brief Return a connection to the pool.
         *
         * @details If the provided connection is still open it is pushed back into the internal queue.
         *          Otherwise an attempt is made to re-create a new \c pqxx::connection using the stored connection string.
         *
         * @param connection Unique pointer to the connection being returned.
         */
        void returnConnection(std::unique_ptr<pqxx::connection> connection);

        std::shared_ptr<SmartHome::Utils::Logger> mpLogger;

        std::string mConnectionStr; ///< Connection string used to open connections
        std::queue<std::unique_ptr<pqxx::connection> > mConnections; ///< Pool of available connections
        std::mutex mConnectionsMutex; ///< Protects \c mConnections
        std::condition_variable mConnectionsCond; ///< Signals availability of connections
    };
}
