#include "database_api.h"

#include <set>

#include "database_service.h"

namespace SmartHomeDB {
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

        if (!params.contains(sj::ParamsKeys::TARGET) || params[sj::ParamsKeys::TARGET] != msTARGET_STR) {
            throw std::runtime_error("Missing or invalid 'target' field");
        }

        if (!params.contains(sj::ParamsKeys::METHOD_PARAMS)) {
            throw std::runtime_error("Missing 'method_params' field");
        }

        const auto &methodParams = params[sj::ParamsKeys::METHOD_PARAMS];

        if (!methodParams.contains(ak::TABLE_STR) || !methodParams[ak::TABLE_STR].is_string()) {
            throw std::runtime_error("Missing or invalid 'table' field");
        }

        const std::string table = methodParams[ak::TABLE_STR];


        if (request.method == ak::GET_STR) {
            return buildSelectQuery(table, methodParams);
        }
        if (request.method == ak::SET_STR) {
            if (!methodParams.contains(sk::WHERE_STR) && !methodParams.contains(ak::VALUES_STR)) {
                throw std::runtime_error("SET requires either 'where' (for UPDATE) or full 'values' (for INSERT)");
            }

            if (methodParams.contains(sk::WHERE_STR)) {
                return buildUpdateQuery(table, methodParams);
            }

            return buildInsertQuery(table, methodParams);
        }
        if (request.method == ak::DELETE_STR) {
            return buildDeleteQuery(table, methodParams);
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
                    pLogger->errorf("[DB_API] Failed to parse batch JSON request into API request: %s]", e.what());
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
                        if (!id.hasValue()) continue;
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
                if (!apiId.hasValue()) return;
                const auto responseJson = dbResultToApiJson(std::move(result), apiId);
                if (responseJson.empty()) {
                    pLogger->debug("[DB_API] Ignoring empty response");
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

    DatabaseClient::DbQuery DatabaseApi::buildSelectQuery(const std::string &table, const nlohmann::json &params) {
        DatabaseClient::DbQuery query;

        query.sql = toUpper(sk::SELECT_STR) + " ";

        if (params.contains(ak::AGGREGATES_STR) && params.contains(ak::COLUMNS_STR)) {
            throw std::runtime_error("Cannot use both 'aggregates' and 'columns'");
        }

        if (params.contains(ak::AGGREGATES_STR)) {
            query.sql += buildAggregates(params[ak::AGGREGATES_STR]);
        } else if (params.contains(ak::COLUMNS_STR)) {
            query.sql += buildColumns(params[ak::COLUMNS_STR]);
        } else {
            query.sql += sk::WILDCARD_STR;
        }

        query.sql += " " + toUpper(sk::FROM_STR) + " " + sqlIdentifier(table);

        if (params.contains(ak::WHERE_STR)) {
            int paramIndex = 1;
            query.sql += " " + toUpper(sk::WHERE_STR);
            query.sql += " " + buildWhereClause(params[ak::WHERE_STR], query.params, paramIndex);
        }

        if (params.contains(ak::ORDER_BY_STR)) {
            query.sql += " " + toUpper(sk::ORDER_BY_STR) + " " + buildOrderBy(params[ak::ORDER_BY_STR]);
        }

        if (params.contains(ak::LIMIT_STR)) {
            if (!params[ak::LIMIT_STR].is_number_integer()) {
                throw std::runtime_error("'limit' must be an integer");
            }

            const int limit = params[ak::LIMIT_STR];
            if (limit < 0) {
                throw std::runtime_error("'limit' must be non-negative");
            }
            query.sql += " " + toUpper(sk::LIMIT_STR) + " " + std::to_string(limit);
        }

        const auto pLogger = DatabaseService::Instance().pLogger;
        pLogger->debugf("[DB_API] [BUILD_SELECT] SQL: %s", query.sql.c_str());
        return query;
    }

    DatabaseClient::DbQuery DatabaseApi::buildInsertQuery(const std::string &table, const nlohmann::json &params) {
        if (!params.contains(ak::VALUES_STR) || !params[ak::VALUES_STR].is_object()) {
            throw std::runtime_error("INSERT requires 'values' object");
        }

        if (params[ak::VALUES_STR].empty()) {
            throw std::runtime_error("'values' cannot be empty");
        }

        DatabaseClient::DbQuery query;
        int paramIndex = 1;

        const auto &values = params[ak::VALUES_STR];

        std::vector<std::string> columns;
        std::vector<std::string> placeholders;

        for (const auto &[key,value]: values.items()) {
            columns.push_back(sqlIdentifier(key));
            placeholders.push_back(addParam(value, query.params, paramIndex));
        }

        query.sql = toUpper(sk::INSERT_INTO_STR) + " " + sqlIdentifier(table) + " (";
        query.sql += joinStrings(columns, ", ");
        query.sql += ") " + toUpper(sk::VALUES_STR) + " (";
        query.sql += joinStrings(placeholders, ", ");
        query.sql += ")";

        if (params.contains(ak::RETURNING_STR)) {
            query.sql += buildReturning(params[ak::RETURNING_STR]);
        }

        const auto pLogger = DatabaseService::Instance().pLogger;
        pLogger->debugf("[DB_API] [BUILD_INSERT] SQL: %s", query.sql.c_str());
        return query;
    }

    DatabaseClient::DbQuery DatabaseApi::buildUpdateQuery(const std::string &table, const nlohmann::json &params) {
        if (!params.contains(ak::VALUES_STR) || !params[ak::VALUES_STR].is_object()) {
            throw std::runtime_error("UPDATE requires 'values' object");
        }

        if (params[ak::VALUES_STR].empty()) {
            throw std::runtime_error("'values' cannot be empty");
        }

        if (!params.contains(ak::WHERE_STR)) {
            throw std::runtime_error("UPDATE requires 'where' object");
        }

        DatabaseClient::DbQuery query;
        int paramIndex = 1;

        const auto &values = params[ak::VALUES_STR];

        std::vector<std::string> setParts;
        for (const auto &[key, value]: values.items()) {
            std::string placeholder = addParam(value, query.params, paramIndex);
            setParts.push_back(sqlIdentifier(key) + " " + sk::EQUAL_STR.data() + " " + placeholder);
        }

        query.sql = toUpper(sk::UPDATE_STR) + " " + sqlIdentifier(table) + " " + toUpper(sk::SET_STR) + " ";
        query.sql += joinStrings(setParts, ", ");
        query.sql += " " + toUpper(sk::WHERE_STR);
        query.sql += " " + buildWhereClause(params[ak::WHERE_STR], query.params, paramIndex);

        if (params.contains(ak::RETURNING_STR)) {
            query.sql += " " + buildReturning(params[ak::RETURNING_STR]);
        }

        const auto pLogger = DatabaseService::Instance().pLogger;
        pLogger->debugf("[DB_API] [BUILD_UPDATE] SQL: %s", query.sql.c_str());
        return query;
    }

    DatabaseClient::DbQuery DatabaseApi::buildDeleteQuery(const std::string &table, const nlohmann::json &params) {
        if (!params.contains(ak::WHERE_STR)) {
            throw std::runtime_error("DELETE requires 'where' object");
        }

        DatabaseClient::DbQuery query;
        int paramIndex = 1;

        query.sql = toUpper(sk::DELETE_STR) + " " + toUpper(sk::FROM_STR) + " " + sqlIdentifier(table);
        query.sql += " " + toUpper(sk::WHERE_STR);
        query.sql += " " + buildWhereClause(params[ak::WHERE_STR], query.params, paramIndex);

        if (params.contains(ak::RETURNING_STR)) {
            query.sql += " " + buildReturning(params[ak::RETURNING_STR]);
        }

        const auto pLogger = DatabaseService::Instance().pLogger;
        pLogger->debugf("[DB_API] [BUILD_DELETE] SQL: %s", query.sql.c_str());
        return query;
    }

    std::string DatabaseApi::buildSubSelect(const nlohmann::json &subSelect, pqxx::params &params, int &paramIndex) {
        if (!subSelect.is_object()) {
            throw std::runtime_error("'$subselect' must be an object");
        }

        if (!subSelect.contains(ak::TABLE_STR) || !subSelect[ak::TABLE_STR].is_string()) {
            throw std::runtime_error("Subselect requires 'table' field");
        }

        std::string sql = "(" + toUpper(sk::SELECT_STR) + " ";

        if (subSelect.contains(ak::COLUMNS_STR)) {
            sql += buildColumns(subSelect[ak::COLUMNS_STR]);
        } else {
            sql += sk::WILDCARD_STR;
        }

        sql += " " + toUpper(sk::FROM_STR) + " " + sqlIdentifier(subSelect[ak::TABLE_STR]);

        if (subSelect.contains(ak::WHERE_STR)) {
            sql += " " + toUpper(sk::WHERE_STR) + " " + buildWhereClause(subSelect[ak::WHERE_STR], params, paramIndex);
        }

        if (subSelect.contains(ak::ORDER_BY_STR)) {
            sql += " " + toUpper(sk::ORDER_BY_STR) + " " + buildOrderBy(subSelect[ak::ORDER_BY_STR]);
        }

        if (subSelect.contains(ak::LIMIT_STR)) {
            if (!subSelect[ak::LIMIT_STR].is_number_integer()) {
                throw std::runtime_error("Subselect 'limit' field must be an integer");
            }
            sql += " " + toUpper(sk::LIMIT_STR) + " " + std::to_string(subSelect[ak::LIMIT_STR].get<int>());
        } else {
            // By default, set limit on subselect to 1
            sql += " " + toUpper(sk::LIMIT_STR) + " 1";
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
                auto isNullStr = joinStrings({sk::IS_STR.data(), sk::NULL_STR.data()}, " ");
                conditions.push_back(column + " " + toUpper(isNullStr));
            } else if (value.is_object()) {
                for (const auto &[op, val]: value.items()) {
                    if (!sk::OPERATORS.contains(toLower(op))) {
                        throw std::runtime_error("Unsupported operator: " + op);
                    }

                    if (val.is_null()) {
                        if (op == sk::NOT_EQUAL_STR) {
                            auto isNotNullStr = joinStrings(
                                {sk::IS_STR.data(), sk::NOT_STR.data(), sk::NULL_STR.data()}, " ");
                            conditions.push_back(column + " " + toUpper(isNotNullStr));
                        } else {
                            throw std::runtime_error("Only '!=' operator allowed with NULL");
                        }
                    } else {
                        std::string placeholder = addParam(val, params, paramIndex);
                        conditions.push_back(column + " " + op + " " + placeholder);
                    }
                }
            } else {
                std::string placeholder = addParam(value, params, paramIndex);
                conditions.push_back(column + " " + sk::EQUAL_STR.data() + " " + placeholder);
            }
        }

        // TODO consider rework to implement OR operator
        return joinStrings(conditions, " "s + sk::AND_STR.data() + " ");
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

            if (colStr == sk::WILDCARD_STR) {
                if (func != sk::COUNT_STR) {
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

            if (!item.contains(ak::COLUMN_STR) || !item[ak::COLUMN_STR].is_string()) {
                throw std::runtime_error("'order_by' items must have 'column' field");
            }

            std::string column = sqlIdentifier(item[ak::COLUMN_STR].get<std::string>());
            std::string order = toUpper(sk::ASC_STR); // default ascending

            if (item.contains(ak::ORDER_STR)) {
                if (!item[ak::ORDER_STR].is_string()) {
                    throw std::runtime_error("'order' must be a string");
                }
                order = toUpper(item[ak::ORDER_STR].get<std::string>());
                if (order != toUpper(sk::ASC_STR) && order != toUpper(sk::DESC_STR)) {
                    throw std::runtime_error("'order' must be 'DESC' or 'ASC'");
                }
            }

            parts.push_back(column + " " + order);
        }

        return joinStrings(parts, ", ");
    }

    std::string DatabaseApi::buildReturning(const nlohmann::json &returning) {
        std::string result = " " + toUpper(sk::RETURNING_STR) + " ";

        if (returning.is_string() && returning == sk::WILDCARD_STR) {
            result += sk::WILDCARD_STR;
        } else if (returning.is_array()) {
            result += buildColumns(returning);
        } else {
            throw std::runtime_error("'returning' must be a non-empty array of columns or '*'");
        }

        return result;
    }

    std::string DatabaseApi::addParam(const nlohmann::json &value, pqxx::params &params, int &paramIndex) {
        // Handle special case - subselect
        if (value.is_object() && value.contains(ak::SUBSELECT_STR)) {
            return buildSubSelect(value[ak::SUBSELECT_STR], params, paramIndex);
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
        } else if (value.is_object()) {
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
