#pragma once
#include <set>

#include "api.h"
#include "database_client.h"

namespace SmartHomeDB {
    using namespace std::string_literals;

    namespace sa = SmartHome::API;
    namespace sj = SmartHome::JsonRpcStrings;


    namespace SqlKeywords {
        // Keys
        inline constexpr std::string_view SELECT_STR = "select";
        inline constexpr std::string_view INSERT_INTO_STR = "insert into";
        inline constexpr std::string_view UPDATE_STR = "update";
        inline constexpr std::string_view DELETE_STR = "delete";
        inline constexpr std::string_view FROM_STR = "from";
        inline constexpr std::string_view WHERE_STR = "where";
        inline constexpr std::string_view ORDER_BY_STR = "order by";
        inline constexpr std::string_view LIMIT_STR = "limit";
        inline constexpr std::string_view RETURNING_STR = "returning";
        inline constexpr std::string_view VALUES_STR = "values";
        inline constexpr std::string_view SET_STR = "set";

        // Values
        inline constexpr std::string_view ASC_STR = "asc";
        inline constexpr std::string_view DESC_STR = "desc";
        inline constexpr std::string_view IS_STR = "is";
        inline constexpr std::string_view NOT_STR = "not";
        inline constexpr std::string_view NULL_STR = "null";
        inline constexpr std::string_view TRUE_STR = "true";
        inline constexpr std::string_view FALSE_STR = "false";
        inline constexpr std::string_view WILDCARD_STR = "*";

        //Aggregates
        inline constexpr std::string_view COUNT_STR = "count";
        inline constexpr std::string_view AVG_STR = "avg";
        inline constexpr std::string_view MAX_STR = "max";
        inline constexpr std::string_view MIN_STR = "min";

        inline const std::set AGGREGATES = {
            COUNT_STR, AVG_STR, MAX_STR, MIN_STR
        };

        // Operators
        inline constexpr std::string_view EQUAL_STR = "=";
        inline constexpr std::string_view NOT_EQUAL_STR = "!=";
        inline constexpr std::string_view GREATER_STR = ">";
        inline constexpr std::string_view LESS_STR = "<";
        inline constexpr std::string_view GREATER_OR_EQUAL_STR = ">=";
        inline constexpr std::string_view LESS_OR_EQUAL_STR = "<=";
        inline constexpr std::string_view LIKE_STR = "like";

        inline const std::set OPERATORS = {
            EQUAL_STR, NOT_EQUAL_STR, GREATER_STR, LESS_STR, GREATER_OR_EQUAL_STR, LESS_OR_EQUAL_STR, LIKE_STR
        };

        // Reserved operators
        inline constexpr std::string_view AND_STR = "and";
        inline constexpr std::string_view OR_STR = "or";
    }

    namespace ApiKeywords {
        inline constexpr std::string_view SET_STR = "set";
        inline constexpr std::string_view GET_STR = "get";
        inline constexpr std::string_view DELETE_STR = "delete";
        inline constexpr std::string_view SUBSELECT_STR = "$subselect";

        inline constexpr std::string_view TABLE_STR = "table";
        inline constexpr std::string_view COLUMNS_STR = "columns";
        inline constexpr std::string_view COLUMN_STR = "column";
        inline constexpr std::string_view AGGREGATES_STR = "aggregates";
        inline constexpr std::string_view WHERE_STR = "where";
        inline constexpr std::string_view ORDER_BY_STR = "order_by";
        inline constexpr std::string_view ORDER_STR = "order";
        inline constexpr std::string_view LIMIT_STR = "limit";
        inline constexpr std::string_view VALUES_STR = "values";
        inline constexpr std::string_view RETURNING_STR = "returning";
    }

    namespace ak = ApiKeywords;
    namespace sk = SqlKeywords;

    /**
     * @brief Adapter between internal JSON-RPC API and the database client.
     *
     * @details Parses incoming JSON-RPC requests, constructs DatabaseClient::DbQuery objects and
     *          forwards them to the DatabaseClient. Converts DB results into API responses.
     */
    class DatabaseApi : public sa::Api {
    public:
        /**
         * @brief Construct with database client.
         *
         * @param pDbClient Shared pointer to DatabaseClient used to execute queries.
         */
        explicit DatabaseApi(const std::shared_ptr<DatabaseClient> &pDbClient) : mpDatabaseClient(pDbClient) {
        };

        /**
         * @brief Set outgoing message callback.
         *
         * @details The callback will be invoked when the API needs to send messages.
         *
         * @param callback Function called with serialized message string.
         */
        void initialize(const std::function<void(const std::string &message)> &callback);

        /**
         * @brief Convert database query result to API JSON.
         *
         * @param queryResult Result returned by DatabaseClient (moved).
         * @param apiId API request identifier to attach to the response.
         *
         * @return JSON object representing the API response.
         */
        static nlohmann::json dbResultToApiJson(DatabaseClient::DbQueryResult &&queryResult,
                                                SmartHome::API::ApiId apiId);

        /**
          * @brief Convert pqxx::field to JSON value.
          *
          * @param field Database field to convert.
          *
          * @return JSON representation of the field value.
          */
        static nlohmann::json fieldValueToJson(const pqxx::field &field);

        /**
          * @brief Transform an API request into a database query.
          *
          * @param request ApiRequest containing request parameters.
          *
          * @return Prepared DatabaseClient::DbQuery ready for execution.
          *
          * @throws std::runtime_error If request is invalid or missing required fields.
          *
          */
        static DatabaseClient::DbQuery apiRequestToDbQuery(const SmartHome::API::ApiRequest &request);

        /**
         * @brief Handle incoming JSON-RPC message.
         *
         * @details Parses raw message and dispatches resulting queries to the database client.
         *
         * @param connectionId Source connection identifier.
         * @param message Message payload as JSON string.
         *
         * @note Example request formats:
         *       \code{.json}
         *       {
         *         "jsonrpc": "2.0",
         *         "method": "database.get",
         *         "params": {
         *           "table": "modules",
         *           "columns": ["logic_address", "config"],
         *           "where": {"id": 1}
         *         },
         *         "id": 42
         *       }
         *       \endcode
         *       \code{.json}
         *       {
         *         "jsonrpc": "2.0",
         *         "method": "database.set",
         *         "params": {
         *           "table": "modules",
         *           "values": {"last_online": "2026-02-05T12:00:00Z"},
         *           "where": {"id": 1}
         *         },
         *         "id": 43
         *       }
         *       \endcode
         *       \code{.json}
         *       {
         *         "jsonrpc": "2.0",
         *         "method": "database.delete",
         *         "params": {
         *           "table": "logs",
         *           "where": {"id": 10}
         *         },
         *         "id": 44
         *       }
         *       \endcode
         */
        void handleIncoming(SmartHome::connectionId_t connectionId, std::string &&message) override;

        /**
        * @brief Handle outgoing API message.
        *
        * @details Forwards serialized message to previously configured callback.
        *
        * @param connectionId Destination connection identifier (unused in client).
        * @param message Message payload to send.
        */
        void handleOutgoing(SmartHome::connectionId_t connectionId, std::string &&message) override;

    private:
        /**
         * @brief Build SELECT query from JSON parameters.
         *
         * @param table Table name.
         * @param params JSON object with select parameters (columns, where, order_by, limit, aggregates, returning).
         *
         * @return Prepared DatabaseClient::DbQuery.
         */
        static DatabaseClient::DbQuery buildSelectQuery(const std::string &table, const nlohmann::json &params);

        /**
         * @brief Build INSERT query from JSON parameters.
         *
         * @param table Table name.
         * @param params JSON containing 'values' and optional 'returning'.
         *
         * @return Prepared DatabaseClient::DbQuery.
         */
        static DatabaseClient::DbQuery buildInsertQuery(const std::string &table, const nlohmann::json &params);

        /**
         * @brief Build UPDATE query from JSON parameters.
         *
         * @param table Table name.
         * @param params JSON with 'values' and 'where', optional 'returning'.
         *
         * @return Prepared DatabaseClient::DbQuery.
         */
        static DatabaseClient::DbQuery buildUpdateQuery(const std::string &table, const nlohmann::json &params);

        /**
         * @brief Build DELETE query from JSON parameters.
         *
         * @param table Table name.
         * @param params JSON containing required 'where' and optional 'returning'.
         *
         * @return Prepared DatabaseClient::DbQuery.
         */
        static DatabaseClient::DbQuery buildDeleteQuery(const std::string &table, const nlohmann::json &params);

        /**
         * @brief Build SQL fragment for a subselect.
         *
         * @param subSelect JSON defining the subselect (table, columns, where, order_by, limit).
         * @param params pqxx parameters container passed by reference.
         * @param paramIndex Next parameter index (modified by the function).
         *
         * @return SQL string for the subselect wrapped in parentheses.
         */
        static std::string buildSubSelect(const nlohmann::json &subSelect, pqxx::params &params, int &paramIndex);

        /**
         * @brief Build WHERE clause from JSON description.
         *
         * @param where JSON describing conditions.
         * @param params pqxx params container.
         * @param paramIndex Parameter index (modified).
         *
         * @return SQL fragment following the WHERE keyword.
         */
        static std::string buildWhereClause(const nlohmann::json &where,
                                            pqxx::params &params,
                                            int &paramIndex);

        /**
         * @brief Build comma-separated column list from JSON array.
         *
         * @param columns Array of column names.
         *
         * @return SQL string with columns separated by commas.
         */
        static std::string buildColumns(const nlohmann::json &columns);

        /**
         * @brief Build list of aggregate functions.
         *
         * @param aggregates Object mapping aggregate function names to columns.
         *
         * @return SQL fragment containing aggregate expressions.
         */
        static std::string buildAggregates(const nlohmann::json &aggregates);

        /**
         * @brief Build ORDER BY clause from JSON array.
         *
         * @param orderBy Array of objects with keys { column, order }.
         *
         * @return SQL ORDER BY fragment.
         */
        static std::string buildOrderBy(const nlohmann::json &orderBy);

        /**
          * @brief Build RETURNING clause.
          *
          * @param returning Either '*' or an array of column names.
          *
          * @return SQL fragment containing RETURNING.
          */
        static std::string buildReturning(const nlohmann::json &returning);

        /**
         * @brief Append a parameter to pqxx::params and return its placeholder.
         *
         * @param value JSON value to append.
         * @param params pqxx params container.
         * @param paramIndex Parameter index (modified).
         *
         * @return Placeholder string used in SQL (e.g. "$1").
         */
        static std::string addParam(const nlohmann::json &value, pqxx::params &params, int &paramIndex);

        /**
         * TODO add full implementation
         * @brief Normalize SQL identifier.
         *
         * @param identifier Column or table name.
         *
         * @return Identifier to use in SQL (currently passthrough).
         */
        static std::string sqlIdentifier(const std::string &identifier);

        /**
         * @brief Join vector of strings using separator.
         *
         * @param vec Vector of strings.
         * @param sep Separator string.
         *
         * @return Joined string.
         */
        static std::string joinStrings(const std::vector<std::string> &vec, const std::string &sep);

        /**
         * @brief Convert to uppercase.
         *
         * @param str Input string view.
         *
         * @return Uppercased string.
         */
        static std::string toUpper(std::string_view str);

        /**
         * @brief Convert to lowercase.
         *
         * @param str Input string view.
         *
         * @return Lowercased string.
         */
        static std::string toLower(std::string_view str);

        static constexpr std::string_view msTARGET_STR = "database";


        std::shared_ptr<DatabaseClient> mpDatabaseClient; ///< Database client used to execute queries
        std::function<void(const std::string &message)> mCallback; ///< Outgoing message callback
    };
}
