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
        inline constexpr std::string_view IS_STR = "is";
        inline constexpr std::string_view NOT_STR = "not";
        inline constexpr std::string_view NULL_STR = "null";
        inline constexpr std::string_view TRUE_STR = "true";
        inline constexpr std::string_view FALSE_STR = "false";
        inline constexpr std::string_view WILDCARD = "*";

        //Aggregates
        inline constexpr std::string_view COUNT = "count";
        inline constexpr std::string_view AVG_STR = "avg";
        inline constexpr std::string_view MAX_STR = "max";
        inline constexpr std::string_view MIN_STR = "min";

        inline const std::set AGGREGATES = {
            COUNT, AVG_STR, MAX_STR, MIN_STR
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
        inline constexpr std::string_view AND_STR = "and";
        inline constexpr std::string_view OR_STR = "or";

    }

    namespace ApiKeywords {
        inline constexpr std::string_view SET = "set";
        inline constexpr std::string_view GET = "get";
        inline constexpr std::string_view DELETE_STR = "delete";

        inline constexpr std::string_view TABLE = "table";
        inline constexpr std::string_view COLUMNS = "columns";
        inline constexpr std::string_view COLUMN = "column";
        inline constexpr std::string_view AGGREGATES = "aggregates";
        inline constexpr std::string_view WHERE = "where";
        inline constexpr std::string_view ORDER_BY = "order_by";
        inline constexpr std::string_view ORDER = "order";
        inline constexpr std::string_view LIMIT = "limit";
        inline constexpr std::string_view VALUES = "values";
        inline constexpr std::string_view RETURNING = "returning";
    }

    namespace ak = ApiKeywords;
    namespace sk = SqlKeywords;

    class DatabaseApi : public sa::Api {
    public:
        explicit DatabaseApi(const std::shared_ptr<DatabaseClient> &pDbClient) : mpDatabaseClient(pDbClient) {
        };

        void initialize(const std::function<void(const std::string &message)> &callback);

        static nlohmann::json dbResultToApiJson(DatabaseClient::DbQueryResult &&queryResult, SmartHome::apiId_t apiId);

        static nlohmann::json fieldValueToJson(const pqxx::field &field);

        static DatabaseClient::DbQuery apiRequestToDbQuery(const SmartHome::API::ApiRequest &request);

        void handleIncoming(SmartHome::connectionId_t connectionId, std::string &&message) override;

        void handleOutgoing(SmartHome::connectionId_t connectionId, std::string &&message) override;

    private:
        static DatabaseClient::DbQuery buildSelectQuery(const std::string &table, const nlohmann::json &params);

        static DatabaseClient::DbQuery buildInsertQuery(const std::string &table, const nlohmann::json &params);

        static DatabaseClient::DbQuery buildUpdateQuery(const std::string &table, const nlohmann::json &params);

        static DatabaseClient::DbQuery buildDeleteQuery(const std::string &table, const nlohmann::json &params);

        static std::string buildWhereClause(const nlohmann::json &where,
                                            pqxx::params &params,
                                            int &paramIndex);

        static std::string buildColumns(const nlohmann::json &columns);

        static std::string buildAggregates(const nlohmann::json &aggregates);

        static std::string buildOrderBy(const nlohmann::json &orderBy);

        static std::string buildReturning(const nlohmann::json &returning);

        static std::string addParam(const nlohmann::json &value, pqxx::params &params, int &paramIndex);

        static std::string sqlIdentifier(const std::string &identifier);

        static std::string joinStrings(const std::vector<std::string> &vec, const std::string &sep);

        static std::string toUpper(std::string_view str);

        static std::string toLower(std::string_view str);

        static constexpr std::string_view msTARGET_STR = "database";


        std::shared_ptr<DatabaseClient> mpDatabaseClient;
        std::function<void(const std::string &message)> mCallback;
    };
}
