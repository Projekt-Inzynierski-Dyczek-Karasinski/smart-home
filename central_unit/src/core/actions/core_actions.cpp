#include "core_actions.h"
#include "database_actions.h"
#include "constants.h"

#include <boost/algorithm/string/case_conv.hpp>

#include "mediator_actions.h"


namespace SmartHome {
    namespace jp = JsonRpcStrings::ParamsKeys;
    namespace cct = Constants::CoreTypes;
    namespace cmt = Constants::MediatorTypes;

    awaitOptApiResponse CoreActions::coreEchoHandler(const cmdMetaPtr &commandMetadata) {
        Core::Instance().mpLogger->debug("[CORE_ACTIONS] [ECHO] called");
        const auto &command = commandMetadata->command;
        API::ApiResponse commandResult;
        commandResult.id = command.commandId;

        std::string message;

        if (command.params.has_value()) {
            const auto &params = command.params.value();
            message = params.dump();
        }

        commandResult.result = "Core Echo response: " + message;

        co_return commandResult;
    }

    awaitOptApiResponse CoreActions::coreGetHandler(const cmdMetaPtr &commandMetadata) {
        Core::Instance().mpLogger->debug("[CORE_ACTIONS] [GET] called");
        const auto &command = commandMetadata->command;
        API::ApiResponse commandResult;
        API::ApiError error;
        commandResult.id = command.commandId;

        error.code = API::ErrorCodes::INVALID_PARAMS;
        error.message = API::errorCodeToString(error.code);
        if (!command.params.has_value()) {
            error.data = "Core get requires params: none received";
            commandResult.error = error;
            co_return commandResult;
        }

        const auto &params = command.params.value();
        if (!params.is_object()) {
            error.data = "Core get requires params object";
            commandResult.error = error;
            co_return commandResult;
        }

        if (!params.contains(jp::TYPE) || !params.at(jp::TYPE).is_string()) {
            error.data = "Core get requires type parameter of string type";
            commandResult.error = error;
            co_return commandResult;
        }

        const auto iter = msGetTypeHandlersRegistry.find(params.at(jp::TYPE).get<std::string_view>());
        if (iter == msGetTypeHandlersRegistry.end()) {
            error.data = "Unsupported core get type: " + params.at(jp::TYPE).get<std::string>();
            commandResult.error = error;
            co_return commandResult;
        }

        Core::Instance().mpLogger->debugf("[TEST] params: %s", params.dump().c_str());
        const auto handlerResult = co_await iter->second(commandMetadata, params);

        if (handlerResult.has_value()) {
            co_return handlerResult;
        }

        error.code = API::ErrorCodes::INTERNAL_ERROR;
        error.message = API::errorCodeToString(error.code);
        error.data = "Failed to get value for type: " + params.at(jp::TYPE).get<std::string>();
        commandResult.error = error;

        co_return commandResult;
    }

    awaitOptApiResponse CoreActions::coreSetHandler(const cmdMetaPtr &commandMetadata) {
        Core::Instance().mpLogger->debug("[CORE_ACTIONS] [SET] called");
        const auto &command = commandMetadata->command;
        API::ApiResponse commandResult;
        API::ApiError error;
        commandResult.id = command.commandId;


        error.code = API::ErrorCodes::INVALID_PARAMS;
        error.message = API::errorCodeToString(error.code);
        if (!command.params.has_value()) {
            error.data = "Core set requires params: none received";
            commandResult.error = error;
            co_return commandResult;
        }

        const auto &params = command.params.value();
        if (!params.is_object()) {
            error.data = "Core set requires params object";
            commandResult.error = error;
            co_return commandResult;
        }

        std::optional<std::string> keyStr;
        std::optional<std::string> valueStr;

        for (const auto &[key, value]: params.items()) {
            if (key == JsonRpcStrings::ParamsKeys::ARGS) continue;
            if (value.is_string()) {
                if (keyStr.has_value()) {
                    error.data = "Core set requires exactly one key=value pair";
                    commandResult.error = error;
                    co_return commandResult;
                }
                keyStr = key;
                valueStr = value.get<std::string>();
            }
        }

        if (!keyStr.has_value() || !valueStr.has_value()) {
            error.data = "Core set requires single key=value pair in params";
            commandResult.error = error;
            co_return commandResult;
        }

        SetKeys key;
        std::string value;

        try {
            key = stringToSetKey(keyStr.value());
            value = valueStr.value();
        } catch (const std::exception &e) {
            error.data = e.what();
            commandResult.error = error;
            co_return commandResult;
        }

        switch (key) {
            case SetKeys::CONNECTION_TYPE:
                if (setConnectionType(commandMetadata, value)) break;
                error.code = API::ErrorCodes::INTERNAL_ERROR;
                error.message = API::errorCodeToString(error.code);
                error.data = "Failed to set connection type";
                commandResult.error = error;
                co_return commandResult;
            case SetKeys::UNDEFINED:
            default:
                error.data = "Undefined set key";
                commandResult.error = error;
                co_return commandResult;
        }

        commandResult.result = nlohmann::json::object({{setKeyToString(key), value}}).dump();

        co_return commandResult;
    }

    awaitOptApiResponse CoreActions::coreNotifyHandler(const cmdMetaPtr &commandMetadata) {
        Core::Instance().mpLogger->debug("[CORE_ACTIONS] [NOTIFY] called");
        const auto &command = commandMetadata->command;


        if (!command.params.has_value() ||
            !command.params->contains(jp::TYPE) ||
            !command.params->at(jp::TYPE).is_string()) {
            Core::Instance().mpLogger->warning(
                "[CORE_ACTIONS] [NOTIFY] Received notification with missing or invalid type parameter");
            co_return std::nullopt;
        }

        const auto &typeParam = command.params->at(jp::TYPE);
        // Handle default db trigger notification
        if (typeParam == Constants::DatabaseTypes::MODULES_CHANGED) {
            co_await DatabaseActions::fetchModulesConfigs();
            co_return std::nullopt;
        }

        if (typeParam == Constants::DatabaseTypes::SENSORS_CHANGED) {
            co_await DatabaseActions::fetchSensorsConfigs();
            Core::Instance().scheduler().loadFromCache();
            co_return std::nullopt;
        }

        // TODO implement handling module mediator notifications
        // Module mediator notifications
        if (typeParam == Constants::MediatorTypes::MANUAL_TRIGGER) {
            Core::Instance().mpLogger->infof(
                "[CORE_ACTIONS] [NOTIFY] Manual trigger notification received with data: %s",
                command.params->dump().c_str());
        }

        if (typeParam == Constants::MediatorTypes::POWER_LOSS) {
            Core::Instance().mpLogger->infof(
                "[CORE_ACTIONS] [NOTIFY] Power loss notification received with data: %s",
                command.params->dump().c_str());
        }

        if (typeParam == Constants::MediatorTypes::ALERT) {
            Core::Instance().mpLogger->infof(
                "[CORE_ACTIONS] [NOTIFY] Alert notification received with data: %s",
                command.params->dump().c_str());
        }

        co_return std::nullopt;
    }

    std::string_view CoreActions::setKeyToString(const SetKeys setKey) {
        switch (setKey) {
            case SetKeys::CONNECTION_TYPE:
                return Constants::CoreTypes::CONNECTION_TYPE;
            case SetKeys::UNDEFINED:
            default:
                return Constants::Common::UNDEFINED;
        }
    }

    CoreActions::SetKeys CoreActions::stringToSetKey(const std::string_view setKey) {
        std::map<std::string_view, SetKeys> setKeyMap = {
            {Constants::CoreTypes::CONNECTION_TYPE, SetKeys::CONNECTION_TYPE}
        };

        const auto iter = setKeyMap.find(boost::algorithm::to_lower_copy(std::string(setKey)));
        return (iter != setKeyMap.end()) ? iter->second : SetKeys::UNDEFINED;
    }

    std::optional<std::unordered_set<connectionId_t> > CoreActions::findConnections(
        std::string_view connectionTypeString) {
        std::shared_lock lock(Actions::msConnectionTypeMapLock);

        const auto target = sai::Target(connectionTypeString.data());
        const auto iter = Actions::msConnectionTypeMap.find(target.type);
        if (iter != Actions::msConnectionTypeMap.end() && !iter->second.empty()) {
            return iter->second;
        }

        return std::nullopt;
    }

    bool CoreActions::setConnectionType(const cmdMetaPtr &pMetadata,
                                        std::string_view connectionTypeString) {
        clearStaleConnectionTypes();

        connectionId_t connectionId;
        // Get Connection ID
        try {
            std::scoped_lock lock(Actions::msActiveRequestsLock);
            connectionId = Actions::msActiveRequests.at(pMetadata->requestId).request.connectionId;
        } catch (...) {
            return false;
        }

        // Set connection type
        {
            const auto connectionType = sai::Target(connectionTypeString.data()).type;
            std::scoped_lock lock(Actions::msConnectionsMapLock, Actions::msConnectionTypeMapLock);

            Actions::msConnectionsMap[connectionId] = connectionType;
            Actions::msConnectionTypeMap[connectionType].insert(connectionId);
        }

        return true;
    }

    void CoreActions::clearStaleConnectionTypes() {
        std::scoped_lock lock(Actions::msConnectionsMapLock, Actions::msConnectionTypeMapLock);

        const auto activeConnections = IPC::SocketServer::Instance().getActiveConnections();

        const auto connectionsMapCopy = Actions::msConnectionsMap;
        for (const auto &id: connectionsMapCopy | std::views::keys) {
            if (!activeConnections.contains(id)) Actions::msConnectionsMap.erase(id);
        }

        const auto connectionTypeMapCopy = Actions::msConnectionTypeMap;
        for (const auto &[connectionType, idSet]: connectionTypeMapCopy) {
            std::unordered_set<connectionId_t> intersectionResult;
            std::ranges::set_intersection(idSet, activeConnections,
                                          std::inserter(intersectionResult, intersectionResult.begin()));

            if (intersectionResult.empty()) {
                Actions::msConnectionTypeMap.erase(connectionType);
                continue;
            }
            Actions::msConnectionTypeMap[connectionType] = intersectionResult;
        }
    }

    CoreActions::ValidationResult<uint> CoreActions::requireModuleId(const nlohmann::json &params) {
        if (!params.contains(jp::MODULE_ID) ||
            !params.at(jp::MODULE_ID).is_number_integer()) {
            return std::unexpected(API::ApiError(
                API::ErrorCodes::INVALID_PARAMS,
                errorCodeToString(API::ErrorCodes::INVALID_PARAMS),
                "Module ID is required and must be an unsigned integer in 'module_id' parameter"));
        }

        const int value = params.at(jp::MODULE_ID).get<uint>();
        return value >= 0 ? static_cast<uint>(value) : 0;
    }

    CoreActions::ValidationResult<uint> CoreActions::requireLimitArg(const nlohmann::json &params) {
        if (!params.contains(jp::ARGS) ||
            !params.at(jp::ARGS).is_array() ||
            params.at(jp::ARGS).empty() ||
            !params.at(jp::ARGS).back().is_number_integer()) {
            return std::unexpected(API::ApiError(
                API::ErrorCodes::INVALID_PARAMS,
                errorCodeToString(API::ErrorCodes::INVALID_PARAMS),
                "Requires limit argument as unsigned integer in last element of 'args' array parameter"));
        }

        const int value = params.at(jp::ARGS).back().get<uint>();
        return value >= 0 ? static_cast<uint>(value) : 0;
    }

    CoreActions::ValidationResult<uint> CoreActions::requireSensorLogicIdArg(const nlohmann::json &params) {
        if (!params.contains(jp::ARGS) ||
            !params.at(jp::ARGS).is_array() ||
            params.at(jp::ARGS).empty() ||
            !params.at(jp::ARGS).front().is_number_integer()) {
            return std::unexpected(API::ApiError(
                API::ErrorCodes::INVALID_PARAMS,
                errorCodeToString(API::ErrorCodes::INVALID_PARAMS),
                "Requires sensor logic ID argument as unsigned integer in first element of 'args' array parameter"));
        }

        const int value = params.at(jp::ARGS).front().get<uint>();
        return value >= 0 ? static_cast<uint>(value) : 0;
    }

    CoreActions::ValidationResult<uint> CoreActions::resolveSensorId(const nlohmann::json &params) {
        if (!params.contains(jp::ARGS) ||
            !params.at(jp::ARGS).is_array() ||
            params.at(jp::ARGS).empty() ||
            !params.at(jp::ARGS).front().is_number_integer()) {
            return std::unexpected(API::ApiError(
                API::ErrorCodes::INVALID_PARAMS,
                errorCodeToString(API::ErrorCodes::INVALID_PARAMS),
                "Requires sensor ID as unsigned integer in first element of 'args' array parameter, "
                "or module ID in 'module_id' parameter with sensor logic ID as first element of 'args' array"));
        }
        int value = 0;

        if (params.contains(jp::MODULE_ID) &&
            params.at(jp::MODULE_ID).is_number_integer()) {
            value = params.at(jp::MODULE_ID).get<uint>();
            const auto moduleId = value >= 0 ? static_cast<uint>(value) : 0;

            value = params.at(jp::ARGS).front().get<uint>();
            const auto logicId = value >= 0 ? static_cast<uint>(value) : 0;
            const auto sensorIdOpt = Core::Instance().configCache().findSensorId(moduleId, logicId);

            if (!sensorIdOpt.has_value()) {
                return std::unexpected(API::ApiError(
                    API::ErrorCodes::NOT_FOUND,
                    errorCodeToString(API::ErrorCodes::NOT_FOUND),
                    "Sensor logic_id [" + std::to_string(logicId) +
                    "] not found for module [" + std::to_string(moduleId) + "]"));
            }
            return sensorIdOpt.value();
        }

        value = params.at(jp::ARGS).front().get<uint>();
        return value >= 0 ? static_cast<uint>(value) : 0;
    }

    ba::awaitable<CoreActions::ValidationResult<nlohmann::json> > CoreActions::queryDatabase(
        const nlohmann::json &dbQuery) {
        API::ApiRequest request;
        const API::InternalApi::Target target(API::InternalApi::TargetTypes::DATABASE);
        const API::InternalApi::Method method(API::InternalApi::MethodTypes::GET);
        request.method = API::getTargetMethodString(target.to_string(), method.to_string());
        request.params = dbQuery;
        request.id = Actions::getNextId();
        auto response = co_await DatabaseActions::sendRequestToDbService(std::move(request));

        // Handle db response
        if (response.error.has_value()) {
            co_return std::unexpected(API::ApiError(
                API::ErrorCodes::INTERNAL_ERROR,
                errorCodeToString(API::ErrorCodes::INTERNAL_ERROR),
                "Database query failed: " + response.error.value().data));
        }

        if (!response.result.has_value() || !nlohmann::json::accept(response.result.value())) {
            co_return std::unexpected(API::ApiError(
                API::ErrorCodes::INTERNAL_ERROR,
                errorCodeToString(API::ErrorCodes::INTERNAL_ERROR),
                "Invalid database response"));
        }

        auto parsedResult = nlohmann::json::parse(response.result.value());
        if (!parsedResult.contains(jp::ROWS) || !parsedResult[jp::ROWS].is_array()) {
            co_return std::unexpected(API::ApiError(
                API::ErrorCodes::INTERNAL_ERROR,
                errorCodeToString(API::ErrorCodes::INTERNAL_ERROR),
                "Database response missing rows array"));
        }

        co_return parsedResult[jp::ROWS];
    }

    awaitOptApiResponse CoreActions::getModules(const cmdMetaPtr &cmdMetadata, const nlohmann::json &params) {
        API::ApiResponse handlerResult;
        handlerResult.id = cmdMetadata->command.commandId;

        const auto modules = Core::Instance().configCache().getAllModules();

        if (modules.empty()) {
            handlerResult.error = API::ApiError(
                API::ErrorCodes::NOT_FOUND,
                errorCodeToString(API::ErrorCodes::NOT_FOUND),
                "No modules found");
            co_return handlerResult;
        }

        nlohmann::json result = nlohmann::json::object();
        nlohmann::json modulesJsonArray = nlohmann::json::array();
        for (const auto &module: modules) {
            modulesJsonArray.push_back(module.to_json());
        }

        result[cct::MODULES] = modulesJsonArray;
        handlerResult.result = to_string(result);

        co_return handlerResult;
    }

    awaitOptApiResponse CoreActions::getModule(const cmdMetaPtr &cmdMetadata, const nlohmann::json &params) {
        API::ApiResponse handlerResult;
        handlerResult.id = cmdMetadata->command.commandId;

        const auto moduleId = requireModuleId(params);
        if (!moduleId.has_value()) {
            handlerResult.error = moduleId.error();
            co_return handlerResult;
        }

        const auto moduleOpt = Core::Instance().configCache().getModule(moduleId.value());

        if (!moduleOpt.has_value()) {
            handlerResult.error = API::ApiError(
                API::ErrorCodes::NOT_FOUND,
                errorCodeToString(API::ErrorCodes::NOT_FOUND),
                "Module with [" + std::to_string(moduleId.value()) + "] ID not found");
            co_return handlerResult;
        }

        nlohmann::json result = nlohmann::json::object();
        result[cct::MODULE] = moduleOpt.value().to_json();
        handlerResult.result = to_string(result);
        co_return handlerResult;
    }

    awaitOptApiResponse CoreActions::getModuleSensors(const cmdMetaPtr &cmdMetadata, const nlohmann::json &params) {
        API::ApiResponse handlerResult;
        handlerResult.id = cmdMetadata->command.commandId;

        const auto moduleId = requireModuleId(params);
        if (!moduleId.has_value()) {
            handlerResult.error = moduleId.error();
            co_return handlerResult;
        }

        const auto sensors = Core::Instance().configCache().getModuleSensors(moduleId.value());

        if (sensors.empty()) {
            handlerResult.error = API::ApiError(
                API::ErrorCodes::NOT_FOUND,
                errorCodeToString(API::ErrorCodes::NOT_FOUND),
                "No sensors found for module with [" + std::to_string(moduleId.value()) + "] ID");
            co_return handlerResult;
        }

        nlohmann::json result = nlohmann::json::object();
        nlohmann::json sensorsJsonArray = nlohmann::json::array();
        for (const auto &sensor: sensors) {
            sensorsJsonArray.push_back(sensor.to_json());
        }

        result[cct::MODULE_SENSORS] = sensorsJsonArray;
        handlerResult.result = to_string(result);
        co_return handlerResult;
    }

    awaitOptApiResponse CoreActions::getSensors(const cmdMetaPtr &cmdMetadata, const nlohmann::json &params) {
        API::ApiResponse handlerResult;
        handlerResult.id = cmdMetadata->command.commandId;

        const auto &sensors = Core::Instance().configCache().getAllSensors();

        if (sensors.empty()) {
            handlerResult.error = API::ApiError(
                API::ErrorCodes::NOT_FOUND,
                errorCodeToString(API::ErrorCodes::NOT_FOUND),
                "No sensors found");
            co_return handlerResult;
        }

        nlohmann::json result = nlohmann::json::object();
        nlohmann::json sensorsJsonArray = nlohmann::json::array();
        for (const auto &sensor: sensors) {
            sensorsJsonArray.push_back(sensor.to_json());
        }

        result[cct::SENSORS] = sensorsJsonArray;
        handlerResult.result = to_string(result);

        co_return handlerResult;
    }

    awaitOptApiResponse CoreActions::getSensor(const cmdMetaPtr &cmdMetadata, const nlohmann::json &params) {
        API::ApiResponse handlerResult;
        handlerResult.id = cmdMetadata->command.commandId;

        const auto sensorId = resolveSensorId(params);
        if (!sensorId.has_value()) {
            handlerResult.error = sensorId.error();
            co_return handlerResult;
        }

        const auto sensorOpt = Core::Instance().configCache().getSensor(sensorId.value());
        if (!sensorOpt.has_value()) {
            handlerResult.error = API::ApiError(
                API::ErrorCodes::NOT_FOUND,
                errorCodeToString(API::ErrorCodes::NOT_FOUND),
                "Sensor with [" + std::to_string(sensorId.value()) + "] ID not found");
            co_return handlerResult;
        }

        nlohmann::json result = nlohmann::json::object();
        result[cct::SENSOR] = sensorOpt.value().to_json();
        handlerResult.result = to_string(result);
        co_return handlerResult;
    }

    awaitOptApiResponse CoreActions::getSensorReadings(const cmdMetaPtr &cmdMetadata, const nlohmann::json &params) {
        API::ApiResponse handlerResult;
        handlerResult.id = cmdMetadata->command.commandId;

        const auto sensorId = resolveSensorId(params);
        if (!sensorId.has_value()) {
            handlerResult.error = sensorId.error();
            co_return handlerResult;
        }

        if (params.at(jp::ARGS).size() < 2) {
            handlerResult.error = API::ApiError(
                API::ErrorCodes::INVALID_PARAMS,
                errorCodeToString(API::ErrorCodes::INVALID_PARAMS),
                "Requires sensor logic ID as first element and limit as second element of 'args' array parameter, "
                "or module ID in 'module_id' parameter with sensor logic ID as first element."
                "Requires limit as last element of 'args' array");
            co_return handlerResult;
        }

        const auto limit = requireLimitArg(params);
        if (!limit.has_value()) {
            handlerResult.error = limit.error();
            co_return handlerResult;
        }
        if (limit.value() == 1) {
            const auto readingOpt = Core::Instance().readingsCache().get(sensorId.value());
            // If reading is in cache return it, otherwise delegate to db
            if (readingOpt.has_value()) {
                nlohmann::json result = nlohmann::json::object();
                result[cct::SENSOR_READINGS] = nlohmann::json::array({readingOpt.value().to_json()});
                handlerResult.result = to_string(result);
                co_return handlerResult;
            }
        }

        // Delegate to db
        API::ApiRequest request;

        nlohmann::json dbQuery = {
            {jp::TABLE, "sensor_readings"},
            {jp::COLUMNS, nlohmann::json::array({"value_text", "value_numeric", "timestamp", "metadata"})},
            {jp::WHERE, {{"sensor_id", sensorId.value()}}},
            {jp::ORDER_BY, nlohmann::json::array({{{"column", "timestamp"}, {"order", "DESC"}}})},
            {jp::LIMIT, limit.value()}
        };

        const auto rows = co_await queryDatabase(dbQuery);
        if (!rows.has_value()) {
            handlerResult.error = rows.error();
            co_return handlerResult;
        }
        nlohmann::json result = nlohmann::json::object();
        nlohmann::json readingsJsonArray = nlohmann::json::array();
        for (const auto &row: rows.value()) {
            try {
                nlohmann::json reading = {
                    {"sensor_id", sensorId.value()},
                    {"value", row["value_numeric"].is_null() ? row["value_text"] : row["value_numeric"]},
                    {"timestamp", row["timestamp"]},
                    {"metadata", row["metadata"]}
                };

                readingsJsonArray.push_back(reading);
            } catch (const std::exception &e) {
                Core::Instance().mpLogger->errorf(
                    "[CORE_ACTIONS] [GET_SENSOR_READINGS] Failed to parse sensor reading from database response: %s",
                    e.what());
            }
        }

        result[cct::SENSOR_READINGS] = readingsJsonArray;
        handlerResult.result = to_string(result);
        co_return handlerResult;
    }


    awaitOptApiResponse CoreActions::getLogs(const cmdMetaPtr &cmdMetadata, const nlohmann::json &params) {
        API::ApiResponse handlerResult;
        handlerResult.id = cmdMetadata->command.commandId;

        const auto moduleId = requireModuleId(params);
        if (!moduleId.has_value()) {
            handlerResult.error = moduleId.error();
            co_return handlerResult;
        }

        const auto limit = requireLimitArg(params);
        if (!limit.has_value()) {
            handlerResult.error = limit.error();
            co_return handlerResult;
        }

        // Delegate to db
        API::ApiRequest request;

        nlohmann::json dbQuery = {
            {jp::TABLE, "logs"},
            {jp::COLUMNS, nlohmann::json::array({"type", "content", "timestamp"})},
            {jp::WHERE, {{"module_id", moduleId.value()}}},
            {jp::ORDER_BY, nlohmann::json::array({{{"column", "timestamp"}, {"order", "DESC"}}})},
            {jp::LIMIT, limit.value()}
        };

        const auto rows = co_await queryDatabase(dbQuery);
        if (!rows.has_value()) {
            handlerResult.error = rows.error();
            co_return handlerResult;
        }

        nlohmann::json result = nlohmann::json::object();
        nlohmann::json readingsJsonArray = nlohmann::json::array();

        for (const auto &row: rows.value()) {
            try {
                nlohmann::json log = {
                    {"type", row["type"]},
                    {"content", row["content"]},
                    {"timestamp", row["timestamp"]}
                };
                readingsJsonArray.push_back(log);
            } catch (const std::exception &e) {
                Core::Instance().mpLogger->errorf(
                    "[CORE_ACTIONS] [GET_LOGS] Failed to parse log from database response: %s",
                    e.what());
            }
        }

        result[cct::LOGS] = readingsJsonArray;
        handlerResult.result = to_string(result);
        co_return handlerResult;
    }

    awaitOptApiResponse CoreActions::getDebugCache(const cmdMetaPtr &cmdMetadata, const nlohmann::json &params) {
        API::ApiResponse handlerResult;
        handlerResult.id = cmdMetadata->command.commandId;

        nlohmann::json result = nlohmann::json::object();
        result["modules_cache"] = nlohmann::json::array();
        for (const auto &module: Core::Instance().configCache().getAllModules()) {
            result["modules_cache"].push_back(module.to_json());
        }
        result["sensors_cache"] = nlohmann::json::array();
        for (const auto &sensor: Core::Instance().configCache().getAllSensors()) {
            result["sensors_cache"].push_back(sensor.to_json());
        }
        result["readings_cache"] = nlohmann::json::array();
        for (const auto &reading: Core::Instance().readingsCache().getDebugAll()) {
            result["readings_cache"].push_back(reading.to_json());
        }

        handlerResult.result = to_string(result);
        co_return handlerResult;
    }

    awaitOptApiResponse CoreActions::getReadingValue(const cmdMetaPtr &cmdMetadata, const nlohmann::json &params) {
        API::ApiResponse handlerResult;
        handlerResult.id = cmdMetadata->command.commandId;

        const auto sensorId = resolveSensorId(params);
        if (!sensorId.has_value()) {
            handlerResult.error = sensorId.error();
            co_return handlerResult;
        }

        const auto sensorOpt = Core::Instance().configCache().getSensor(sensorId.value());
        if (!sensorOpt.has_value()) {
            handlerResult.error = API::ApiError(
                API::ErrorCodes::NOT_FOUND,
                errorCodeToString(API::ErrorCodes::NOT_FOUND),
                "Sensor with [" + std::to_string(sensorId.value()) + "] ID not found in cache");
            co_return handlerResult;
        }

        const auto &sensor = sensorOpt.value();
        const uint moduleId = sensor.moduleId;
        const uint sensorLogicId = sensor.logicId;

        const auto type = params.at(jp::TYPE).get<std::string>();
        const bool forceRefresh = params.contains(jp::FORCE) &&
                                  params.at(jp::FORCE).is_boolean() &&
                                  params.at(jp::FORCE).get<bool>();

        // Try cache if not forced refresh
        if (!forceRefresh) {
            if (sensorId.has_value()) {
                if (const auto cached = Core::Instance().readingsCache().getFresh(sensorId.value())) {
                    handlerResult.result = cached->value.dump();
                    co_return handlerResult;
                }
            }
        }

        nlohmann::json mediatorArgs = nlohmann::json::array({sensorLogicId});

        // Delegate to mediator
        auto response = co_await MediatorActions::sendToModule(cmdMetadata, moduleId, type, mediatorArgs,
                                                               API::InternalApi::MethodTypes::GET);

        if (response.result.has_value()) handlerResult.result = std::move(response.result);
        else handlerResult.error = std::move(response.error);

        co_return handlerResult;
    }

    awaitOptApiResponse CoreActions::getModuleInfo(const cmdMetaPtr &cmdMetadata, const nlohmann::json &params) {
        API::ApiResponse handlerResult;
        handlerResult.id = cmdMetadata->command.commandId;

        const auto moduleId = requireModuleId(params);
        if (!moduleId.has_value()) {
            handlerResult.error = moduleId.error();
            co_return handlerResult;
        }

        const auto type = params.at(jp::TYPE).get<std::string>();

        nlohmann::json mediatorArgs = nlohmann::json::array();
        if (params.contains(jp::ARGS) && params.at(jp::ARGS).is_array()) {
            mediatorArgs = params.at(jp::ARGS);
        }

        // Delegate to mediator
        auto response = co_await MediatorActions::sendToModule(cmdMetadata, moduleId.value(), type, mediatorArgs,
                                                               API::InternalApi::MethodTypes::GET);

        if (response.result.has_value()) handlerResult.result = std::move(response.result);
        else handlerResult.error = std::move(response.error);

        co_return handlerResult;
    }

    awaitOptApiResponse CoreActions::getModuleLogs(const cmdMetaPtr &cmdMetadata, const nlohmann::json &params) {
        API::ApiResponse handlerResult;
        handlerResult.id = cmdMetadata->command.commandId;

        const auto moduleId = requireModuleId(params);
        if (!moduleId.has_value()) {
            handlerResult.error = moduleId.error();
            co_return handlerResult;
        }

        const auto limit = requireLimitArg(params);
        if (!limit.has_value()) {
            handlerResult.error = limit.error();
            co_return handlerResult;
        }

        const auto type = params.at(jp::TYPE).get<std::string>();

        // Delegate to mediator
        auto response = co_await MediatorActions::sendToModule(cmdMetadata, moduleId.value(), type, params.at(jp::ARGS),
                                                               API::InternalApi::MethodTypes::GET);

        if (response.result.has_value()) handlerResult.result = std::move(response.result);
        else handlerResult.error = std::move(response.error);

        co_return handlerResult;
    }


    std::unordered_map<std::string_view, CoreActions::getTypeHandler>
    CoreActions::msGetTypeHandlersRegistry{
        // Core types
        {cct::MODULES, getModules},
        {cct::MODULE, getModule},
        {cct::MODULE_SENSORS, getModuleSensors},
        {cct::SENSORS, getSensors},
        {cct::SENSOR, getSensor},
        // Debug types
        {cct::_DEBUG_CACHE, getDebugCache},
        // Core types delegated to database actions
        {cct::SENSOR_READINGS, getSensorReadings},
        {cct::LOGS, getLogs},
        // Mediator types
        {cmt::ACTUATOR_VALUE, getReadingValue},
        {cmt::SENSOR_VALUE, getReadingValue},
        {cmt::FORCE_READ_SENSOR_VALUE, getReadingValue},
        {cmt::BATTERY_LEVEL, getModuleInfo},
        {cmt::SENSOR_LIST, getModuleInfo},
        {cmt::MODULE_LOGS, getModuleLogs},
    };
}
