#pragma once
#include <memory>
#include <pqxx/pqxx>

namespace SmartHomeDB {
    class DatabaseConnectionManager;

    /**
     * @brief RAII wrapper for a database connection obtained from the connection manager.
     *
     * @details Owns a unique pointer to a pqxx::connection and returns the connection to the
     *          DatabaseConnectionManager when destroyed or moved out.
     *          The type is move-only to ensure single ownership of the underlying connection.
     */
    class DatabaseConnection {
        DatabaseConnectionManager &mDbConnectionManager;
        std::unique_ptr<pqxx::connection> mpConnection;

    public:
        /**
         * @brief Construct a DatabaseConnection.
         *
         * @details Takes ownership of an existing pqxx::connection and stores a reference to the
         *          connection manager so the connection can be returned on destruction or reassignment.
         *
         * @param dbConnectionManager Reference to the manager that maintains the connection pool.
         * @param connection Unique pointer to an active pqxx::connection.
         */
        DatabaseConnection(DatabaseConnectionManager &dbConnectionManager,
                           std::unique_ptr<pqxx::connection> connection);

        /**
         * @brief Destroy the DatabaseConnection.
         *
         * @details If the underlying pqxx::connection is still owned, it will be returned to the
         *          DatabaseConnectionManager so the connection can be reused.
         */
        ~DatabaseConnection();

        DatabaseConnection(const DatabaseConnection &) = delete;

        DatabaseConnection &operator=(const DatabaseConnection &) = delete;

        /**
         * @brief Move-construct a DatabaseConnection.
         *
         * @details Transfers ownership of the contained pqxx::connection from \p other. The
         *          reference to the DatabaseConnectionManager is preserved.
         *
         * @param other Rvalue reference to the source DatabaseConnection.
         */
        DatabaseConnection(DatabaseConnection &&other) noexcept;

        /**
         * @brief Move-assign a DatabaseConnection.
         *
         * @details Returns any currently-owned connection to the manager, then transfers
         *          ownership from other to this object.
         *
         * @param other Rvalue reference to the source DatabaseConnection.
         *
         * @return Reference to the assigned object.
         */
        DatabaseConnection &operator=(DatabaseConnection &&other) noexcept;

        /**
         * @brief Access the underlying pqxx::connection by reference.
         *
         * @return Reference to the underlying pqxx::connection.
         */
        pqxx::connection &operator*() const noexcept;

        /**
         * @brief Pointer-like access to the underlying pqxx::connection.
         *
         * @return Pointer to the underlying pqxx::connection.
         */
        pqxx::connection *operator->() const noexcept;
    };
}
