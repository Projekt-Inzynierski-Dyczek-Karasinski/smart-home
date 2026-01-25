#pragma once
#include <memory>
#include <pqxx/pqxx>

namespace SmartHomeDB {
    class DatabaseConnectionManager;
    class DatabaseConnection {
        DatabaseConnectionManager &mDbConnectionManager;
        std::unique_ptr<pqxx::connection> mpConnection;

    public:
        DatabaseConnection(DatabaseConnectionManager &dbConnectionManager, std::unique_ptr<pqxx::connection> connection);

        ~DatabaseConnection();

        DatabaseConnection(const DatabaseConnection&) = delete;
        DatabaseConnection& operator=(const DatabaseConnection&) = delete;

        DatabaseConnection(DatabaseConnection&&) noexcept;
        DatabaseConnection& operator=(DatabaseConnection&&) noexcept;

        pqxx::connection &operator*();
        pqxx::connection *operator->() ;
    };
}
