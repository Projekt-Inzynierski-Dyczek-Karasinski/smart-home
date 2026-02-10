#include "database_api.h"
#include "database_service.h"
#include "constants.h"

#include <set>


namespace SmartHomeDB {
    namespace sc = SmartHome::Constants;

    void DatabaseApi::initialize(const std::function<void(const std::string &message)> &callback) {
        mCallback = callback;
    }

    nlohmann::json DatabaseApi::dbResultToApiJson(DatabaseClient::DbQueryResult &&queryResult,
                                                  const SmartHome::API::ApiId apiId) {
        sa::ApiResponse response;
        response.id = apiId;

        if (queryResult.error.has_value()) {
            sa::ApiError error;
            error.code = SmartHome::API::ErrorCodes::INTERNAL_ERROR;
            error.message = SmartHome::API::errorCodeToString(error.code);
            error.data = queryResult.error.value();

            response.error = error;
            return response.to_json();
        }

        const auto &resultSql = queryResult.result;

        nlohmann::json resultJson;
        resultJson["affected_rows"] = resultSql.affected_rows();
        resultJson["rows"] = nlohmann::json::array();

        auto &rowsJsonArray = resultJson["rows"];

        for (const auto &row: resultSql) {
            nlohmann::json rowJson;

            for (int col = 0; col < row.size(); col++) {
                const std::string colName = resultSql.column_name(col);
                rowJson[colName] = fieldValueToJson(row[col]);
            }
            rowsJsonArray.push_back(rowJson);
        }

        response.result = to_string(resultJson);
        return response.to_json();
    }

    nlohmann::json DatabaseApi::fieldValueToJson(const pqxx::field &field) {
        if (field.is_null()) {
            return nullptr;
        }

        const pqxx::oid typeOid = field.type();

        // Types from postgres "select typname, oid from pg_type;"
        static const std::set<pqxx::oid> sIntegerTypes = {
            20, // int8
            21, // int2
            23, // int4
            26, // oid
        };

        static const std::set<pqxx::oid> sFloatTypes = {
            700, // float4
            701, // float8
            1700, // numeric
        };

        static const std::set<pqxx::oid> sJsonTypes = {
            114, // json
            3802, // jsonb
        };

        try {
            // Bool type
            if (typeOid == 16) {
                return field.as<bool>();
            }

            // Integer types
            if (sIntegerTypes.contains(typeOid)) {
                return field.as<int64_t>();
            }

            // Float types
            if (sFloatTypes.contains(typeOid)) {
                return field.as<double>();
            }

            // JSON types
            if (sJsonTypes.contains(typeOid)) {
                return nlohmann::json::parse(field.c_str());
            }
        } catch (const std::exception &e) {
            DatabaseService::Instance().pLogger->debugf("[DB_API] Failed to parse pqxx into JSON: %s", e.what());
            DatabaseService::Instance().pLogger->debug("[DB_API] Falling back to string representation");
        }

        // Default return as string
        return field.c_str();
    }

    DatabaseClient::DbQuery DatabaseApi::apiRequestToDbQuery(const SmartHome::API::ApiRequest &request) {
        if (!request.params.has_value()) {
            throw std::runtime_error("Missing parameters");
        }

        const auto &params = request.params.value();
        if (!params.is_object()) {
            throw std::runtime_error("Parameters must be an object");
        }

        auto [targetStr, methodStr] = SmartHome::API::parseTargetMethodString(request.method);
        if (toLower(targetStr) != "database") {
            throw std::runtime_error("Invalid target - expected 'database'");
        }

        const auto methodLower = toLower(methodStr);

        if (!params.contains(sjp::TABLE) || !params[sjp::TABLE].is_string()) {
            throw std::runtime_error("Missing or invalid 'table' field");
        }

        const std::string table = params[sjp::TABLE];

        if (methodLower == sc::Methods::GET) {
            return buildSelectQuery(table, params);
        }
        if (methodLower == sc::Methods::SET) {
            if (!params.contains(sk::WHERE) && !params.contains(sjp::VALUES)) {
                throw std::runtime_error("SET requires either 'where' (for UPDATE) or full 'values' (for INSERT)");
            }

            if (params.contains(sk::WHERE)) {
                return buildUpdateQuery(table, params);
            }

            return buildInsertQuery(table, params);
        }
        if (methodLower == sc::Methods::DELETE) {
            return buildDeleteQuery(table, params);
        }


        throw std::runtime_error("Unknown method type for database operations");
    }


    void DatabaseApi::handleIncoming(SmartHome::connectionId_t connectionId, std::string &&message) {
        auto pLogger = DatabaseService::Instance().pLogger;
        pLogger->debugf("[DB_API] [HANDLE_INCOMING] Message: %s", message.c_str());

        if (!nlohmann::json::accept(message)) {
            pLogger->error("[DB_API] Failed to accept incoming message: invalid JSON format");
            return;
        }

        nlohmann::json requestJson;
        try {
            requestJson = nlohmann::json::parse(message);
        } catch (const std::exception &e) {
            pLogger->errorf("[DB_API] Failed to parse incoming message: %s", e.what());
            return;
        }


        if (requestJson.is_array()) {
            // Handle batch request
            std::vector<SmartHome::API::ApiId> apiIds;
            std::vector<DatabaseClient::DbQuery> queries;

            sa::ApiRequest requestApi;
            sa::ApiError errorApi;
            // Prepare queries for transaction. All requests must be valid queries to handle batch query.
            for (const auto &item: requestJson) {
                errorApi.code = SmartHome::API::ErrorCodes::INVALID_REQUEST;
                errorApi.message = SmartHome::API::errorCodeToString(errorApi.code);

                try {
                    requestApi(item);
                } catch (const std::exception &e) {
                    pLogger->errorf("[DB_API] Failed to parse batch JSON request into API request: %s", e.what());
                    errorApi.data = "Failed to parse batch JSON request into API request: "s + e.what();
                    break;
                }


                try {
                    queries.push_back(apiRequestToDbQuery(requestApi));
                } catch (const std::exception &e) {
                    pLogger->errorf("[DB_API] Failed to parse API request into SQL query: %s", e.what());

                    errorApi.code = SmartHome::API::ErrorCodes::INTERNAL_ERROR;
                    errorApi.message = SmartHome::API::errorCodeToString(errorApi.code);
                    errorApi.data = "Failed to parse API request into SQL query: "s + e.what();
                    break;
                }


                apiIds.push_back(requestApi.id);
            }

            // Return error if one of requests in batch was invalid
            if (!errorApi.data.empty()) {
                sa::ApiResponse responseApi;
                responseApi.id = nullptr;
                responseApi.error = errorApi;

                handleOutgoing(connectionId, responseApi.to_string());
                return;
            }


            auto handleBatchQueryCallback = [this, apiIds, connectionId, pLogger]
            (DatabaseClient::DbBatchQueryResult &&batchResult) {
                auto resultJsonArray = nlohmann::json::array();

                // Handle transaction error - return error response to every request in batch.
                if (batchResult.transactionError.has_value()) {
                    DatabaseService::Instance().pLogger->errorf("[DB_API] [HANDLE_INCOMING] Transaction error: %s",
                                                                batchResult.transactionError.value().c_str());
                    sa::ApiError callbackErrorApi;
                    sa::ApiResponse callbackResponseApi;

                    callbackErrorApi.code = SmartHome::API::ErrorCodes::INTERNAL_ERROR;
                    callbackErrorApi.message = SmartHome::API::errorCodeToString(callbackErrorApi.code);
                    callbackErrorApi.data = "Transaction Error: "s + batchResult.transactionError.value();

                    callbackResponseApi.error = callbackErrorApi;

                    for (const auto id: apiIds) {
                        if (!id.hasValue()) {
                            pLogger->debug("[DB_API] Skipping response for request with null ID in batch");
                            pLogger->debugf("[DB_API] Skipped response: %s", (callbackResponseApi.to_string().c_str()));
                            continue;
                        }
                        callbackResponseApi.id = id;
                        resultJsonArray.push_back(callbackResponseApi.to_json());
                    }

                    if (resultJsonArray.empty()) return;

                    handleOutgoing(connectionId, to_string(resultJsonArray));
                    return;
                }

                auto resultsVector = batchResult.results;
                for (auto i = 0u; i < resultsVector.size(); i++) {
                    if (!apiIds[i].hasValue()) continue;
                    resultJsonArray.push_back(dbResultToApiJson(std::move(resultsVector[i]), apiIds[i]));
                }

                if (resultJsonArray.empty()) {
                    pLogger->debug("[DB_API] Ignoring empty response");
                    return;
                }

                handleOutgoing(connectionId, to_string(resultJsonArray));
            };

            mpDatabaseClient->handleBatchQuery(queries, handleBatchQueryCallback);
            return;
        }


        SmartHome::API::ApiRequest requestApi;
        SmartHome::API::ApiResponse responseApi;
        SmartHome::API::ApiError errorApi;

        responseApi.id = nullptr;

        errorApi.code = SmartHome::API::ErrorCodes::INVALID_REQUEST;
        errorApi.message = SmartHome::API::errorCodeToString(errorApi.code);

        try {
            requestApi(requestJson);
        } catch (const std::exception &e) {
            pLogger->errorf("[DB_API] Failed to parse singular JSON request into API request: %s", e.what());

            errorApi.data = "Failed to parse singular JSON request into API request: "s + e.what();
            responseApi.error = errorApi;

            handleOutgoing(connectionId, responseApi.to_string());
            return;
        }


        auto apiId = requestApi.id;
        responseApi.id = apiId;

        DatabaseClient::DbQuery query;
        try {
            query = apiRequestToDbQuery(requestApi);
        } catch (const std::exception &e) {
            pLogger->errorf("[DB_API] Failed to parse API request into SQL query: %s", e.what());

            errorApi.code = SmartHome::API::ErrorCodes::INTERNAL_ERROR;
            errorApi.message = SmartHome::API::errorCodeToString(errorApi.code);
            errorApi.data = "Failed to parse API request into SQL query: "s + e.what();

            responseApi.error = errorApi;

            if (responseApi.id.hasValue()) {
                handleOutgoing(connectionId, responseApi.to_string());
            }
            return;
        }


        mpDatabaseClient->handleQuery(
            query, [this, connectionId, apiId, pLogger](DatabaseClient::DbQueryResult &&result) {
                const auto responseJson = dbResultToApiJson(std::move(result), apiId);
                if (responseJson.empty()) {
                    pLogger->debug("[DB_API] Ignoring empty response");
                    return;
                }

                if (!apiId.hasValue()) {
                    pLogger->debug("[DB_API] Skipping response for request with null ID");
                    pLogger->debugf("[DB_API] Skipped response: %s", to_string(responseJson).c_str());
                    return;
                }
                handleOutgoing(connectionId, std::move(to_string(responseJson)));
            });
    }

    void DatabaseApi::handleOutgoing(SmartHome::connectionId_t connectionId, std::string &&message) {
        const auto pLogger = DatabaseService::Instance().pLogger;
        pLogger->debugf("[DB_API] [HANDLE_OUTGOING] Message: %s", message.c_str());
        mCallback(message);
    }

    void DatabaseApi::handleIncomingDbTrigger(std::string &&triggerName, std::string &&triggerData) {
        const auto pLogger = DatabaseService::Instance().pLogger;
        pLogger->debugf("[DB_API] [HANDLE_DB_TRIGGER] Message: %s", triggerName.c_str());

        sa::ApiRequest notification;
        notification.method = "core.notify";

        auto &params = notification.params.emplace(nlohmann::json::object());
        params[sj::ParamsKeys::TYPE] = triggerName;

        // Try to parse trigger data as JSON
        if (nlohmann::json::accept(triggerData)) {
            try {
                params[sj::ParamsKeys::DATA] = nlohmann::json::parse(triggerData);
            } catch (const std::exception &e) {
                pLogger->errorf("[DB_API] Failed to parse trigger data JSON: %s", e.what());
            }
        }
        // Fallback to raw string data
        if (!params.contains(sj::ParamsKeys::DATA)) params[sj::ParamsKeys::DATA] = triggerData;

        handleOutgoing(0, notification.to_string());
    }

    DatabaseClient::DbQuery DatabaseApi::buildSelectQuery(const std::string &table, const nlohmann::json &params) {
        DatabaseClient::DbQuery query;

        query.sql = toUpper(sk::SELECT) + " ";

        if (params.contains(sjp::AGGREGATES) && params.contains(sjp::COLUMNS)) {
            throw std::runtime_error("Cannot use both 'aggregates' and 'columns'");
        }

        if (params.contains(sjp::AGGREGATES)) {
            query.sql += buildAggregates(params[sjp::AGGREGATES]);
        } else if (params.contains(sjp::COLUMNS)) {
            query.sql += buildColumns(params[sjp::COLUMNS]);
        } else {
            query.sql += sk::WILDCARD;
        }

        query.sql += " " + toUpper(sk::FROM) + " " + sqlIdentifier(table);

        if (params.contains(sjp::WHERE)) {
            int paramIndex = 1;
            query.sql += " " + toUpper(sk::WHERE);
            query.sql += " " + buildWhereClause(params[sjp::WHERE], query.params, paramIndex);
        }

        if (params.contains(sjp::ORDER_BY)) {
            query.sql += " " + toUpper(sk::ORDER_BY) + " " + buildOrderBy(params[sjp::ORDER_BY]);
        }

        if (params.contains(sjp::LIMIT)) {
            if (!params[sjp::LIMIT].is_number_integer()) {
                throw std::runtime_error("'limit' must be an integer");
            }

            const int limit = params[sjp::LIMIT];
            if (limit < 0) {
                throw std::runtime_error("'limit' must be non-negative");
            }
            query.sql += " " + toUpper(sk::LIMIT) + " " + std::to_string(limit);
        }

        const auto pLogger = DatabaseService::Instance().pLogger;
        pLogger->debugf("[DB_API] [BUILD_SELECT] SQL: %s", query.sql.c_str());
        return query;
    }

    DatabaseClient::DbQuery DatabaseApi::buildInsertQuery(const std::string &table, const nlohmann::json &params) {
        if (!params.contains(sjp::VALUES) || !params[sjp::VALUES].is_object()) {
            throw std::runtime_error("INSERT requires 'values' object");
        }

        if (params[sjp::VALUES].empty()) {
            throw std::runtime_error("'values' cannot be empty");
        }

        DatabaseClient::DbQuery query;
        int paramIndex = 1;

        const auto &values = params[sjp::VALUES];

        std::vector<std::string> columns;
        std::vector<std::string> placeholders;

        for (const auto &[key,value]: values.items()) {
            columns.push_back(sqlIdentifier(key));
            placeholders.push_back(addParam(value, query.params, paramIndex));
        }

        query.sql = toUpper(sk::INSERT_INTO) + " " + sqlIdentifier(table) + " (";
        query.sql += joinStrings(columns, ", ");
        query.sql += ") " + toUpper(sk::VALUES) + " (";
        query.sql += joinStrings(placeholders, ", ");
        query.sql += ")";

        if (params.contains(sjp::RETURNING)) {
            query.sql += buildReturning(params[sjp::RETURNING]);
        }

        const auto pLogger = DatabaseService::Instance().pLogger;
        pLogger->debugf("[DB_API] [BUILD_INSERT] SQL: %s", query.sql.c_str());
        return query;
    }

    DatabaseClient::DbQuery DatabaseApi::buildUpdateQuery(const std::string &table, const nlohmann::json &params) {
        if (!params.contains(sjp::VALUES) || !params[sjp::VALUES].is_object()) {
            throw std::runtime_error("UPDATE requires 'values' object");
        }

        if (params[sjp::VALUES].empty()) {
            throw std::runtime_error("'values' cannot be empty");
        }

        if (!params.contains(sjp::WHERE)) {
            throw std::runtime_error("UPDATE requires 'where' object");
        }

        DatabaseClient::DbQuery query;
        int paramIndex = 1;

        const auto &values = params[sjp::VALUES];

        std::vector<std::string> setParts;
        for (const auto &[key, value]: values.items()) {
            std::string placeholder = addParam(value, query.params, paramIndex);
            setParts.push_back(sqlIdentifier(key) + " " + sk::EQUAL.data() + " " + placeholder);
        }

        query.sql = toUpper(sk::UPDATE) + " " + sqlIdentifier(table) + " " + toUpper(sk::SET) + " ";
        query.sql += joinStrings(setParts, ", ");
        query.sql += " " + toUpper(sk::WHERE);
        query.sql += " " + buildWhereClause(params[sjp::WHERE], query.params, paramIndex);

        if (params.contains(sjp::RETURNING)) {
            query.sql += " " + buildReturning(params[sjp::RETURNING]);
        }

        const auto pLogger = DatabaseService::Instance().pLogger;
        pLogger->debugf("[DB_API] [BUILD_UPDATE] SQL: %s", query.sql.c_str());
        return query;
    }

    DatabaseClient::DbQuery DatabaseApi::buildDeleteQuery(const std::string &table, const nlohmann::json &params) {
        if (!params.contains(sjp::WHERE)) {
            throw std::runtime_error("DELETE requires 'where' object");
        }

        DatabaseClient::DbQuery query;
        int paramIndex = 1;

        query.sql = toUpper(sk::DELETE) + " " + toUpper(sk::FROM) + " " + sqlIdentifier(table);
        query.sql += " " + toUpper(sk::WHERE);
        query.sql += " " + buildWhereClause(params[sjp::WHERE], query.params, paramIndex);

        if (params.contains(sjp::RETURNING)) {
            query.sql += " " + buildReturning(params[sjp::RETURNING]);
        }

        const auto pLogger = DatabaseService::Instance().pLogger;
        pLogger->debugf("[DB_API] [BUILD_DELETE] SQL: %s", query.sql.c_str());
        return query;
    }

    std::string DatabaseApi::buildSubSelect(const nlohmann::json &subSelect, pqxx::params &params, int &paramIndex) {
        if (!subSelect.is_object()) {
            throw std::runtime_error("'$subselect' must be an object");
        }

        if (!subSelect.contains(sjp::TABLE) || !subSelect[sjp::TABLE].is_string()) {
            throw std::runtime_error("Subselect requires 'table' field");
        }

        std::string sql = "(" + toUpper(sk::SELECT) + " ";

        if (subSelect.contains(sjp::COLUMNS)) {
            sql += buildColumns(subSelect[sjp::COLUMNS]);
        } else {
            sql += sk::WILDCARD;
        }

        sql += " " + toUpper(sk::FROM) + " " + sqlIdentifier(subSelect[sjp::TABLE]);

        if (subSelect.contains(sjp::WHERE)) {
            sql += " " + toUpper(sk::WHERE) + " " + buildWhereClause(subSelect[sjp::WHERE], params, paramIndex);
        }

        if (subSelect.contains(sjp::ORDER_BY)) {
            sql += " " + toUpper(sk::ORDER_BY) + " " + buildOrderBy(subSelect[sjp::ORDER_BY]);
        }

        if (subSelect.contains(sjp::LIMIT)) {
            if (!subSelect[sjp::LIMIT].is_number_integer()) {
                throw std::runtime_error("Subselect 'limit' field must be an integer");
            }
            sql += " " + toUpper(sk::LIMIT) + " " + std::to_string(subSelect[sjp::LIMIT].get<int>());
        } else {
            // By default, set limit on subselect to 1
            sql += " " + toUpper(sk::LIMIT) + " 1";
        }

        sql += ")";

        return sql;
    }

    std::string DatabaseApi::buildWhereClause(const nlohmann::json &where, pqxx::params &params, int &paramIndex) {
        if (!where.is_object() || where.empty()) {
            throw std::runtime_error("'where' must be non-empty object");
        }

        std::vector<std::string> conditions;

        for (const auto &[key, value]: where.items()) {
            std::string column = sqlIdentifier(key);

            if (value.is_null()) {
                auto isNullStr = joinStrings({sk::IS.data(), sk::NULL_STR.data()}, " ");
                conditions.push_back(column + " " + toUpper(isNullStr));
            } else if (value.is_object()) {
                for (const auto &[op, val]: value.items()) {
                    if (!sk::OPERATORS.contains(toLower(op))) {
                        throw std::runtime_error("Unsupported operator: " + op);
                    }

                    if (val.is_null()) {
                        if (op == sk::NOT_EQUAL) {
                            auto isNotNullStr = joinStrings(
                                {sk::IS.data(), sk::NOT.data(), sk::NULL_STR.data()}, " ");
                            conditions.push_back(column + " " + toUpper(isNotNullStr));
                        } else {
                            throw std::runtime_error("Only '!=' operator allowed with NULL");
                        }
                    } else {
                        const std::string placeholder = addParam(val, params, paramIndex);
                        std::string condition = column;
                        condition += " ";
                        condition += op;
                        condition += " ";
                        condition += placeholder;
                        conditions.push_back(std::move(condition));
                    }
                }
            } else {
                const std::string placeholder = addParam(value, params, paramIndex);
                std::string condition = column;
                condition += " ";
                condition += sk::EQUAL.data();
                condition += " ";
                condition += placeholder;
                conditions.push_back(std::move(condition));
            }
        }

        // TODO consider rework to implement OR operator
        return joinStrings(conditions, " "s + sk::AND.data() + " ");
    }

    std::string DatabaseApi::buildColumns(const nlohmann::json &columns) {
        if (!columns.is_array() || columns.empty()) {
            throw std::runtime_error("'columns' must be a non-empty array");
        }

        std::vector<std::string> cols;

        for (const auto &col: columns) {
            if (!col.is_string()) {
                throw std::runtime_error("Column names must be a string");
            }
            cols.push_back(sqlIdentifier(col.get<std::string>()));
        }

        return joinStrings(cols, ", ");
    }

    std::string DatabaseApi::buildAggregates(const nlohmann::json &aggregates) {
        if (!aggregates.is_object() || aggregates.empty()) {
            throw std::runtime_error("'aggregates' must be non-empty object");
        }

        std::vector<std::string> parts;

        for (const auto &[func, column]: aggregates.items()) {
            if (!sk::AGGREGATES.contains(func)) {
                throw std::runtime_error("Unsupported aggregate function: " + func);
            }

            if (!column.is_string()) {
                throw std::runtime_error("Aggregate column must be a string");
            }

            auto colStr = column.get<std::string>();

            if (colStr == sk::WILDCARD) {
                if (func != sk::COUNT) {
                    throw std::runtime_error("Only COUNT function can use '*'");
                }
                parts.push_back(toUpper(func) + "(*)");
            } else {
                parts.push_back(toUpper(func) + "(" + sqlIdentifier(colStr) + ")");
            }
        }

        return joinStrings(parts, ", ");
    }


    std::string DatabaseApi::buildOrderBy(const nlohmann::json &orderBy) {
        if (!orderBy.is_array() || orderBy.empty()) {
            throw std::runtime_error("'order_by' must be a non-empty array");
        }

        std::vector<std::string> parts;

        for (const auto &item: orderBy) {
            if (!item.is_object()) {
                throw std::runtime_error("'order_by' items must be objects");
            }

            if (!item.contains(sjp::COLUMN) || !item[sjp::COLUMN].is_string()) {
                throw std::runtime_error("'order_by' items must have 'column' field");
            }

            const std::string column = sqlIdentifier(item[sjp::COLUMN].get<std::string>());
            std::string order = toUpper(sk::ASC); // default ascending

            if (item.contains(sjp::ORDER)) {
                if (!item[sjp::ORDER].is_string()) {
                    throw std::runtime_error("'order' must be a string");
                }
                order = toUpper(item[sjp::ORDER].get<std::string>());
                if (order != toUpper(sk::ASC) && order != toUpper(sk::DESC)) {
                    throw std::runtime_error("'order' must be 'DESC' or 'ASC'");
                }
            }

            std::string part = column;
            part += " ";
            part += order;
            parts.push_back(std::move(part));
        }

        return joinStrings(parts, ", ");
    }

    std::string DatabaseApi::buildReturning(const nlohmann::json &returning) {
        std::string result = " " + toUpper(sk::RETURNING) + " ";

        if (returning.is_string() && returning == sk::WILDCARD) {
            result += sk::WILDCARD;
        } else if (returning.is_array()) {
            result += buildColumns(returning);
        } else {
            throw std::runtime_error("'returning' must be a non-empty array of columns or '*'");
        }

        return result;
    }

    std::string DatabaseApi::addParam(const nlohmann::json &value, pqxx::params &params, int &paramIndex) {
        // Handle special case - subselect
        if (value.is_object() && value.contains(sjp::SUBSELECT)) {
            return buildSubSelect(value[sjp::SUBSELECT], params, paramIndex);
        }

        std::string placeholder = "$" + std::to_string(paramIndex++);

        if (value.is_null()) {
            params.append();
        } else if (value.is_string()) {
            params.append(value.get<std::string>());
        } else if (value.is_number_integer()) {
            params.append(value.get<int64_t>());
        } else if (value.is_number_float()) {
            params.append(value.get<double>());
        } else if (value.is_boolean()) {
            params.append(value.get<bool>());
        } else if (value.is_object() || value.is_array()) {
            params.append(value.dump());
        } else {
            throw std::runtime_error("Unsupported value type");
        }

        return placeholder;
    }

    std::string DatabaseApi::sqlIdentifier(const std::string &identifier) {
        // TODO add sanitization / whitelist of allowed identifiers
        return identifier;
    }

    std::string DatabaseApi::joinStrings(const std::vector<std::string> &vec, const std::string &sep) {
        if (vec.empty()) return "";

        size_t index = 0;
        std::string result = vec[index++];

        while (index < vec.size()) {
            result += sep + vec[index++];
        }

        return result;
    }

    std::string DatabaseApi::toUpper(const std::string_view str) {
        std::string upper;
        std::ranges::transform(str, std::back_inserter(upper), ::toupper);
        return upper;
    }

    std::string DatabaseApi::toLower(const std::string_view str) {
        std::string lower;
        std::ranges::transform(str, std::back_inserter(lower), ::tolower);
        return lower;
    }
}
