#include "database_client.h"
#include "database_connection.h"
#include "database_service.h"


namespace SmartHomeDB {
    void DatabaseClient::handleQuery(const DbQuery &query, std::function<void(DbQueryResult &&result)> callback) const {
        auto isCancelled = std::make_shared<std::atomic_bool>(false);

        const auto timeoutTimer = std::make_shared<ba::steady_timer>(DatabaseService::Instance().getUtilityIoContext(),
                                                                     msTRANSACTION_TIMEOUT);
        timeoutTimer->async_wait([this, isCancelled, timeoutTimer, callback](const std::error_code &ec) {
            bool expected{false};
            constexpr bool desired{true};
            if (!ec && !isCancelled->compare_exchange_strong(expected, desired, std::memory_order_acq_rel)) {
                DbQueryResult queryResult{.error = "Query timed out"};
                callback(std::move(queryResult));
            }
        });


        mIoContext.post([this, isCancelled, query, callback] {
            // Block until connection is available
            const auto conn = mpDbConnManager->acquireConnection();
            if (isCancelled->load(std::memory_order_acquire)) return;

            pqxx::work txn(*conn);
            pqxx::result result;
            std::string error;

            try {
                result = txn.exec(query.sql, query.params);
                txn.commit();
            } catch (const pqxx::sql_error &e) {
                std::string_view whatStr(e.what());
                const auto newLinePos = whatStr.find('\n');

                error = "SQL Error ("s + std::string(whatStr.substr(0, newLinePos));
                error += ") \nQuery ("s + e.query() + ")";

            } catch (const std::exception &e) {
                error = "Unexpected error: "s + e.what();
            }

            if (!isCancelled->load(std::memory_order_acquire)) {
                DbQueryResult queryResult;
                if (!error.empty()) {
                    queryResult.error = error;
                }
                queryResult.result = result;
                callback(std::move(queryResult));
            }
        });
    }

    void DatabaseClient::handleBatchQuery(const std::vector<DbQuery> &queries,
                                          const std::function<void(DbBatchQueryResult &&batchResult)> &callback) const {
        auto isCancelled = std::make_shared<std::atomic_bool>(false);

        const auto timeoutTimer = std::make_shared<ba::steady_timer>(DatabaseService::Instance().getUtilityIoContext(),
                                                                     msTRANSACTION_TIMEOUT);
        timeoutTimer->async_wait([this, isCancelled, timeoutTimer, callback = callback](const std::error_code &ec) {
            bool expected{false};
            constexpr bool desired{true};
            if (!ec && !isCancelled->compare_exchange_strong(expected, desired, std::memory_order_acq_rel)) {
                DbBatchQueryResult queryResult{.transactionError = "Transaction timed out"};
                callback(std::move(queryResult));
            }
        });


        mIoContext.post([this, isCancelled, queries = queries, callback = callback] {
            // Block until connection is available
            const auto conn = mpDbConnManager->acquireConnection();
            if (isCancelled->load(std::memory_order_acquire)) return;

            pqxx::work txn(*conn);
            pqxx::result result;
            std::string error;
            DbBatchQueryResult queryResult;

            for (const auto &query: queries) {
                if (isCancelled->load(std::memory_order_acquire)) {
                    return; // Cancel without commiting, transaction will be rolled back
                };
                try {
                    result = txn.exec(query.sql, query.params);
                } catch (const pqxx::sql_error &e) {
                    std::string_view whatStr(e.what());
                    const auto newLinePos = whatStr.find('\n');

                    error = "SQL Error ("s + std::string(whatStr.substr(0, newLinePos));
                    error += ") \nQuery ("s + e.query() + ")";

                    break;
                } catch (const std::exception &e) {
                    error = "Unexpected error: "s + e.what();
                    break;
                }
                queryResult.results.push_back(DbQueryResult{.result = result});
            }

            if (error.empty()) {
                txn.commit(); // Commit only when transaction run without errors
            }


            if (!isCancelled->load(std::memory_order_acquire)) {
                if (!error.empty()) {
                    queryResult.transactionError = error;
                }

                callback(std::move(queryResult));
            }
        });
    }
}
