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

    /**
     * @brief Client responsible for executing database queries.
     *
     * @details Provides asynchronous execution of single queries and batches using a
     *          connection manager and the provided Boost.Asio io_context.
     *          Results and errors are returned via callback functions.
     */
    class DatabaseClient {
    public:
        /**
         * @brief Single database query description.
         *
         * @details Contains SQL text with placeholders and a parameter container used when executing the statement.
         */
        struct DbQuery {
            std::string sql;
            pqxx::params params;
        };

        /**
         * @brief Result of a single database query.
         *
         * @details Contains the pqxx::result produced by the query.
         *          On error the optional \c error field will contain an error message.
         */
        struct DbQueryResult {
            pqxx::result result;
            std::optional<std::string> error;
        };

        /**
        * @brief Result of executing a batch of queries inside a single transaction.
        *
        * @details Holds individual results for each executed query and an optional
        *          transaction-level error string when the batch failed or was rolled back.
        */
        struct DbBatchQueryResult {
            std::vector<DbQueryResult> results;
            std::optional<std::string> transactionError;
        };

        /**
        * @brief Construct a DatabaseClient.
        *
        * @details Stores references to the io_context used to schedule work and the
        *          connection manager used to acquire database connections.
        *
        * @param ioContext Reference to Boost.Asio io_context for scheduling async tasks.
        * @param pDbConnManager Shared pointer to DatabaseConnectionManager providing DB connections.
        */
        explicit DatabaseClient(ba::io_context &ioContext,
                                const std::shared_ptr<DatabaseConnectionManager> &pDbConnManager)
            : mIoContext(ioContext),
              mpDbConnManager(pDbConnManager) {
        }

        /**
        * @brief Execute a single database query asynchronously.
        *
        * @details The query will be executed on the client's io_context and the provided
        *          callback will be invoked with the resulting DbQueryResult when complete.
        *          Timeouts and execution errors are reported via the result's \c error field.
        *
        * @param query Description of the SQL query to execute (SQL + parameters).
        * @param callback Callback invoked with the query result (moved).
        */
        void handleQuery(const DbQuery &query, const std::function<void(DbQueryResult &&result)> &callback) const;

        /**
        * @brief Execute multiple queries as a single transaction asynchronously.
        *
        * @details All queries in the provided vector are executed inside a single transaction.
        *          If any query fails, the transaction is rolled back and \c transactionError
        *          will be set on the returned DbBatchQueryResult.
        *          Each successful query's individual result is available in the \c results vector.
        *
        * @param queries Vector of queries to execute within a single transaction.
        * @param callback Callback invoked with the batch result (moved).
        */
        void handleBatchQuery(const std::vector<DbQuery> &queries,
                              const std::function<void(DbBatchQueryResult &&batchResult)> &callback) const;

    private:
        static constexpr auto msTRANSACTION_TIMEOUT = 5s;

        ba::io_context &mIoContext;
        std::shared_ptr<DatabaseConnectionManager> mpDbConnManager; ///< Connection manager for acquiring DB connections
    };
}
