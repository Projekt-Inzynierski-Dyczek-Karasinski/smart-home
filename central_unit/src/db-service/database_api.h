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
        inline constexpr std::string_view SELECT = "select";
        inline constexpr std::string_view INSERT_INTO = "insert into";
        inline constexpr std::string_view UPDATE = "update";
        inline constexpr std::string_view DELETE = "delete";
        inline constexpr std::string_view FROM = "from";
        inline constexpr std::string_view WHERE = "where";
        inline constexpr std::string_view ORDER_BY = "order by";
        inline constexpr std::string_view LIMIT = "limit";
        inline constexpr std::string_view RETURNING = "returning";
        inline constexpr std::string_view VALUES = "values";
        inline constexpr std::string_view SET = "set";

        // Values
        inline constexpr std::string_view ASC = "asc";
        inline constexpr std::string_view DESC = "desc";
        inline constexpr std::string_view IS = "is";
        inline constexpr std::string_view NOT = "not";
        /// \c NULL is a macro in C/C++, \c NULL_STR is used to avoid conflicts
        inline constexpr std::string_view NULL_STR = "null";
        inline constexpr std::string_view TRUE = "true";
        inline constexpr std::string_view FALSE = "false";
        inline constexpr std::string_view WILDCARD = "*";

        //Aggregates
        inline constexpr std::string_view COUNT = "count";
        inline constexpr std::string_view AVG = "avg";
        inline constexpr std::string_view MAX = "max";
        inline constexpr std::string_view MIN = "min";

        inline const std::set AGGREGATES = {
            COUNT, AVG, MAX, MIN
        };

        // Operators
        inline constexpr std::string_view EQUAL = "=";
        inline constexpr std::string_view NOT_EQUAL = "!=";
        inline constexpr std::string_view GREATER = ">";
        inline constexpr std::string_view LESS = "<";
        inline constexpr std::string_view GREATER_OR_EQUAL = ">=";
        inline constexpr std::string_view LESS_OR_EQUAL = "<=";
        inline constexpr std::string_view LIKE = "like";

        inline const std::set OPERATORS = {
            EQUAL, NOT_EQUAL, GREATER, LESS, GREATER_OR_EQUAL, LESS_OR_EQUAL, LIKE
        };

        // Reserved operators
        inline constexpr std::string_view AND = "and";
        inline constexpr std::string_view OR = "or";
    }

    namespace sjp = SmartHome::JsonRpcStrings::ParamsKeys;
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
        }

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

        /**
         * @brief Handle incoming database trigger notification.
         *
         * @details Converts trigger information into API notification and sends it out.
         *
         * @note Notification format example:
         *      \code{.json}
         *      {
         *        "jsonrpc": "2.0",
         *        "method": "core.notify",
         *        "params": {
         *          "type": <triggerName>,
         *          "data": <triggerData>
         *        }
         *      }
         *      \endcode
         *
         * @param triggerName Name of the database trigger event.
         * @param triggerData Associated data payload (e.g. JSON string with details).
         */
        void handleIncomingDbTrigger(std::string &&triggerName, std::string &&triggerData);

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
         * @brief Build a single SET assignment fragment for UPDATE.
         *
         * @details For plain columns generates "column = $N".
         * @details For dot-separated paths (e.g. "config.key") generates a
         *          jsonb_set() expression targeting the nested JSONB field,
         *          with an appropriate type cast derived from the value type.
         *
         * @param key Column name or dot-separated JSONB path.
         * @param value JSON value to assign.
         * @param params pqxx params container (modified).
         * @param paramIndex Next parameter index (modified).
         *
         * @return SQL fragment suitable for use in a SET clause.
         */
        static std::string buildSetPart(const std::string &key,
                                 const nlohmann::json &value,
                                 pqxx::params &params,
                                 int &paramIndex);

        /**
         * @brief Build a SET assignment fragment that removes a value.
         *
         * @details For plain column paths generates "column = NULL".
         * @details For dot-separated JSONB paths ("e.g. config.key") generates a
         *          "column = column #- $N::text[]" expression using the PostgreSQL #- operator
         *          to remove the nested key.
         *
         * @param path Column name or dot-separated JSONB path to the field to remove.
         * @param params pqxx params container (modified).
         * @param paramIndex Next parameter index (modified).
         *
         * @return SQL fragment suitable for use in a SET clause.
         */
        static std::string buildDeleteSetPart(const std::string &path, pqxx::params &params, int &paramIndex);

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
         * @brief Convert a dot-separated JSON path to a PostgreSQL text[] parameter.
         *
         * @details Splits \p jsonPath on '.' and appends it as a text[] array literal
         *          to \p params, then returns the placeholder "$N::text[]" for use in
         *          jsonb_set() or #- expressions.
         *
         * @param jsonPath Dot-separated path (e.g. "schedule.enabled").
         * @param params pqxx params container (modified).
         * @param paramIndex Next parameter index (modified).
         *
         * @return Placeholder string with cast (e.g. "$2::text[]").
         */
        static std::string buildJsonbPathParam(const std::string &jsonPath, pqxx::params &params, int &paramIndex);

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


        std::shared_ptr<DatabaseClient> mpDatabaseClient; ///< Database client used to execute queries
        std::function<void(const std::string &message)> mCallback; ///< Outgoing message callback
    };
}
