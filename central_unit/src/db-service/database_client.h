#pragma once

#include "database_connection_manager.h"

#include <functional>
#include <string>
#include <boost/asio.hpp>
#include <pqxx/pqxx>


namespace SmartHomeDB {
    using namespace std::string_literals;
    using namespace std::chrono_literals;

    namespace ba = boost::asio;

    class DatabaseClient {
    public:
        struct DbQuery {
            std::string sql;
            pqxx::params params;
        };

        struct DbQueryResult {
            pqxx::result result;
            std::optional<std::string> error;
        };

        struct DbBatchQueryResult {
            std::vector<DbQueryResult> results;
            std::optional<std::string> transactionError;
        };

        explicit DatabaseClient(ba::io_context &ioContext,
                                const std::shared_ptr<DatabaseConnectionManager> &pDbConnManager)
            : mIoContext(ioContext),
              mpDbConnManager(pDbConnManager) {
        };

        void handleQuery(const DbQuery &query, std::function<void(DbQueryResult &&result)> callback) const;

        void handleBatchQuery(const std::vector<DbQuery> &queries,
                              const std::function<void(DbBatchQueryResult &&batchResult)>& callback) const;

    private:
        static constexpr auto msTRANSACTION_TIMEOUT = 5s;

        ba::io_context &mIoContext;
        std::shared_ptr<DatabaseConnectionManager> mpDbConnManager;
    };
}
