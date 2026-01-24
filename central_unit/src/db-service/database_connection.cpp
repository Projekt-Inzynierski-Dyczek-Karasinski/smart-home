#include "database_connection.h"
#include "database_connection_manager.h"

namespace SmartHomeDB {
    DatabaseConnection::DatabaseConnection(DatabaseConnectionManager &dbConnectionManager,
                                           std::unique_ptr<pqxx::connection> connection)
        : mDbConnectionManager(dbConnectionManager),
          mpConnection(std::move(connection)) {
    };

    DatabaseConnection::~DatabaseConnection() {
        if (mpConnection) {
            mDbConnectionManager.returnConnection(std::move(mpConnection));
        }
    }

    DatabaseConnection::DatabaseConnection(DatabaseConnection &&other) noexcept
        : mDbConnectionManager(other.mDbConnectionManager),
          mpConnection(std::move(other.mpConnection)) {
    }

    DatabaseConnection &DatabaseConnection::operator=(DatabaseConnection &&other) noexcept {
        if (this != &other) {
            if (mpConnection) {
                mDbConnectionManager.returnConnection(std::move(mpConnection));
            }
            mpConnection = std::move(other.mpConnection);
        }
        return *this;
    }

    pqxx::connection &DatabaseConnection::operator*() {
        return *mpConnection;
    }

    pqxx::connection *DatabaseConnection::operator->() {
        return mpConnection.get();
    }
}
