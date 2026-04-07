#include "core_actions.h"
#include "action_helpers.h"
#include "constants.h"
#include "database_actions.h"
#include "mediator_actions.h"

#include <boost/asio.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/classification.hpp>

namespace SmartHome {
    using namespace std::string_literals;

    // Method handlers

    awaitOptApiResponse CoreActions::coreEchoHandler(const cmdMetaPtr pCommandMetadata) {
        Core::Instance().mpLogger->debug("[CORE_ACTIONS] [ECHO] called");
        const auto &command = pCommandMetadata->command;
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

    awaitOptApiResponse CoreActions::coreGetHandler(const cmdMetaPtr pCommandMetadata) {
        Core::Instance().mpLogger->debug("[CORE_ACTIONS] [GET] called");
        API::ApiResponse commandResult;
        commandResult.id = pCommandMetadata->command.commandId;

        const auto params = ActionHelpers::requireParams(*pCommandMetadata);
        if (!params.has_value()) {
            commandResult.error = params.error();
            co_return commandResult;
        }

        const auto &pParams = params.value();

        const auto type = ActionHelpers::requireType(*pParams);
        if (!type.has_value()) {
            commandResult.error = params.error();
            co_return commandResult;
        }

        const auto iter = msGetTypeHandlersRegistry.find(type.value());
        if (iter == msGetTypeHandlersRegistry.end()) {
            commandResult.error = API::ApiError(
                API::ErrorCodes::INVALID_PARAMS,
                errorCodeToString(API::ErrorCodes::INVALID_PARAMS),
                "Unsupported core get type: " + type.value()
            );
            co_return commandResult;
        }

        const auto handlerResult = co_await iter->second(pCommandMetadata, pParams);

        if (handlerResult.has_value()) {
            co_return handlerResult;
        }

        commandResult.error = API::ApiError(
            API::ErrorCodes::INTERNAL_ERROR,
            errorCodeToString(API::ErrorCodes::INTERNAL_ERROR),
            "Failed to get value for type: " + type.value()
        );

        co_return commandResult;
    }

    awaitOptApiResponse CoreActions::coreSetHandler(const cmdMetaPtr pCommandMetadata) {
        Core::Instance().mpLogger->debug("[CORE_ACTIONS] [SET] called");
        API::ApiResponse commandResult;
        commandResult.id = pCommandMetadata->command.commandId;

        const auto params = ActionHelpers::requireParams(*pCommandMetadata);
        if (!params.has_value()) {
            commandResult.error = params.error();
            co_return commandResult;
        }

        const auto &pParams = params.value();

        const auto type = ActionHelpers::requireType(*pParams);
        if (!type.has_value()) {
            commandResult.error = params.error();
            co_return commandResult;
        }

        const auto iter = msSetTypeHandlersRegistry.find(type.value());
        if (iter == msSetTypeHandlersRegistry.end()) {
            commandResult.error = API::ApiError(
                API::ErrorCodes::INVALID_PARAMS,
                errorCodeToString(API::ErrorCodes::INVALID_PARAMS),
                "Unsupported core set type: " + type.value()
            );
            co_return commandResult;
        }

        const auto handlerResult = co_await iter->second(pCommandMetadata, pParams);

        if (handlerResult.has_value()) {
            co_return handlerResult;
        }

        commandResult.error = API::ApiError(
            API::ErrorCodes::INTERNAL_ERROR,
            errorCodeToString(API::ErrorCodes::INTERNAL_ERROR),
            "Failed to set value for type: " + type.value()
        );

        co_return commandResult;
    }

    awaitOptApiResponse CoreActions::coreDeleteHandler(const cmdMetaPtr pCommandMetadata) {
        Core::Instance().mpLogger->debug("[CORE_ACTIONS] [DELETE] called");
        API::ApiResponse commandResult;
        commandResult.id = pCommandMetadata->command.commandId;

        const auto params = ActionHelpers::requireParams(*pCommandMetadata);
        if (!params.has_value()) {
            commandResult.error = params.error();
            co_return commandResult;
        }

        const auto &pParams = params.value();

        const auto type = ActionHelpers::requireType(*pParams);
        if (!type.has_value()) {
            commandResult.error = type.error();
            co_return commandResult;
        }

        if (type.value() != cct::MODULE && type.value() != cct::DEVICE) {
            commandResult.error = API::ApiError(
                API::ErrorCodes::INVALID_PARAMS,
                errorCodeToString(API::ErrorCodes::INVALID_PARAMS),
                "Unsupported core delete type: " + type.value());
            co_return commandResult;
        }

        co_return co_await deleteDatabaseValue(pCommandMetadata, pParams);
    }

    awaitOptApiResponse CoreActions::coreNotifyHandler(const cmdMetaPtr pCommandMetadata) {
        Core::Instance().mpLogger->debug("[CORE_ACTIONS] [NOTIFY] called");

        const auto params = ActionHelpers::requireParams(*pCommandMetadata);
        if (!params.has_value()) {
            Core::Instance().mpLogger->warning(
                "[CORE_ACTIONS] [NOTIFY] Received notification with missing params object");
            co_return std::nullopt;
        }

        const auto &pParams = params.value();

        const auto type = ActionHelpers::requireType(*pParams);
        if (!params.has_value()) {
            Core::Instance().mpLogger->warning(
                "[CORE_ACTIONS] [NOTIFY] Received notification with missing or invalid type parameter");
            co_return std::nullopt;
        }

        // Handle default db trigger notification
        if (type.value() == Constants::DatabaseTypes::MODULES_CHANGED) {
            co_await DatabaseActions::fetchModulesConfigs();
            co_return std::nullopt;
        }

        if (type.value() == Constants::DatabaseTypes::DEVICES_CHANGED) {
            co_await DatabaseActions::fetchDevicesConfigs();
            Core::Instance().scheduler().loadFromCache();
            co_return std::nullopt;
        }

        // TODO implement handling module mediator notifications
        // Module mediator notifications
        if (type.value() == Constants::MediatorTypes::MANUAL_TRIGGER) {
            Core::Instance().mpLogger->infof(
                "[CORE_ACTIONS] [NOTIFY] Manual trigger notification received with data: %s",
                pParams->dump().c_str());
        }

        if (type.value() == Constants::MediatorTypes::POWER_LOSS) {
            Core::Instance().mpLogger->infof(
                "[CORE_ACTIONS] [NOTIFY] Power loss notification received with data: %s",
                pParams->dump().c_str());
        }

        if (type.value() == Constants::MediatorTypes::ALERT) {
            Core::Instance().mpLogger->infof(
                "[CORE_ACTIONS] [NOTIFY] Alert notification received with data: %s",
                pParams->dump().c_str());
        }

        co_return std::nullopt;
    }


    // Utility

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


    // Private
    // Get type handlers

    awaitOptApiResponse CoreActions::getModules(const cmdMetaPtr pCommandMetadata, jsonPtr) {
        API::ApiResponse handlerResult;
        handlerResult.id = pCommandMetadata->command.commandId;

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

    awaitOptApiResponse CoreActions::getModule(cmdMetaPtr pCommandMetadata, jsonPtr pParams) {
        API::ApiResponse handlerResult;
        handlerResult.id = pCommandMetadata->command.commandId;

        const auto moduleId = ActionHelpers::requireModuleId(*pParams);
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

    awaitOptApiResponse CoreActions::getModuleDevices(cmdMetaPtr pCommandMetadata, jsonPtr pParams) {
        API::ApiResponse handlerResult;
        handlerResult.id = pCommandMetadata->command.commandId;

        const auto moduleId = ActionHelpers::requireModuleId(*pParams);
        if (!moduleId.has_value()) {
            handlerResult.error = moduleId.error();
            co_return handlerResult;
        }

        const auto devices = Core::Instance().configCache().getModuleDevices(moduleId.value());

        if (devices.empty()) {
            handlerResult.error = API::ApiError(
                API::ErrorCodes::NOT_FOUND,
                errorCodeToString(API::ErrorCodes::NOT_FOUND),
                "No devices found for module with [" + std::to_string(moduleId.value()) + "] ID");
            co_return handlerResult;
        }

        nlohmann::json result = nlohmann::json::object();
        nlohmann::json devicesJsonArray = nlohmann::json::array();
        for (const auto &device: devices) {
            devicesJsonArray.push_back(device.to_json());
        }

        result[cct::MODULE_DEVICES] = devicesJsonArray;
        handlerResult.result = to_string(result);
        co_return handlerResult;
    }

    awaitOptApiResponse CoreActions::getDevices(const cmdMetaPtr pCommandMetadata, jsonPtr pParams) {
        API::ApiResponse handlerResult;
        handlerResult.id = pCommandMetadata->command.commandId;

        const auto &devices = Core::Instance().configCache().getAllDevices();

        if (devices.empty()) {
            handlerResult.error = API::ApiError(
                API::ErrorCodes::NOT_FOUND,
                errorCodeToString(API::ErrorCodes::NOT_FOUND),
                "No devices found");
            co_return handlerResult;
        }

        nlohmann::json result = nlohmann::json::object();
        nlohmann::json devicesJsonArray = nlohmann::json::array();
        for (const auto &device: devices) {
            devicesJsonArray.push_back(device.to_json());
        }

        result[cct::DEVICES] = devicesJsonArray;
        handlerResult.result = to_string(result);

        co_return handlerResult;
    }

    awaitOptApiResponse CoreActions::getDevice(cmdMetaPtr pCommandMetadata, jsonPtr pParams) {
        API::ApiResponse handlerResult;
        handlerResult.id = pCommandMetadata->command.commandId;

        const auto deviceId = ActionHelpers::resolveDeviceId(*pParams);
        if (!deviceId.has_value()) {
            handlerResult.error = deviceId.error();
            co_return handlerResult;
        }

        const auto deviceOpt = Core::Instance().configCache().getDevice(deviceId.value());
        if (!deviceOpt.has_value()) {
            handlerResult.error = API::ApiError(
                API::ErrorCodes::NOT_FOUND,
                errorCodeToString(API::ErrorCodes::NOT_FOUND),
                "Device with [" + std::to_string(deviceId.value()) + "] ID not found");
            co_return handlerResult;
        }

        nlohmann::json result = nlohmann::json::object();
        result[cct::DEVICE] = deviceOpt.value().to_json();
        handlerResult.result = to_string(result);
        co_return handlerResult;
    }

    awaitOptApiResponse CoreActions::getDeviceReadings(cmdMetaPtr pCommandMetadata, jsonPtr pParams) {
        API::ApiResponse handlerResult;
        handlerResult.id = pCommandMetadata->command.commandId;

        const auto deviceId = ActionHelpers::resolveDeviceId(*pParams);
        if (!deviceId.has_value()) {
            handlerResult.error = deviceId.error();
            co_return handlerResult;
        }

        if (pParams->at(jp::ARGS).size() < 2) {
            handlerResult.error = API::ApiError(
                API::ErrorCodes::INVALID_PARAMS,
                errorCodeToString(API::ErrorCodes::INVALID_PARAMS),
                "Requires device logic ID as first element and limit as second element of 'args' array parameter, "
                "or module ID in 'module_id' parameter with device logic ID as first element."
                "Requires limit as last element of 'args' array");
            co_return handlerResult;
        }

        const auto limit = ActionHelpers::requireLimitArg(*pParams);
        if (!limit.has_value()) {
            handlerResult.error = limit.error();
            co_return handlerResult;
        }
        if (limit.value() == 1) {
            const auto readingOpt = Core::Instance().readingsCache().get(deviceId.value());
            // If reading is in cache return it, otherwise delegate to db
            if (readingOpt.has_value()) {
                nlohmann::json result = nlohmann::json::object();
                result[cct::DEVICE_READINGS] = nlohmann::json::array({readingOpt.value().to_json()});
                handlerResult.result = to_string(result);
                co_return handlerResult;
            }
        }

        nlohmann::json whereClause = {{"device_id", deviceId.value()}};

        if (pParams->contains(jp::FROM) && pParams->at(jp::FROM).is_string()) {
            whereClause["timestamp"][">="] = pParams->at(jp::FROM).get<std::string>();
        }
        if (pParams->contains(jp::TO) && pParams->at(jp::TO).is_string()) {
            whereClause["timestamp"]["<="] = pParams->at(jp::TO).get<std::string>();
        }

        // Delegate to db
        API::ApiRequest request;

        nlohmann::json dbQuery = {
            {jp::TABLE, "device_readings"},
            {jp::COLUMNS, nlohmann::json::array({"value_text", "value_numeric", "timestamp", "metadata"})},
            {jp::WHERE, whereClause},
            {jp::ORDER_BY, nlohmann::json::array({{{"column", "timestamp"}, {"order", "DESC"}}})},
            {jp::LIMIT, limit.value()}
        };

        const auto dbResponse = co_await ActionHelpers::queryDatabase(Constants::Methods::GET, std::move(dbQuery));
        if (!dbResponse.has_value()) {
            handlerResult.error = dbResponse.error();
            co_return handlerResult;
        }
        nlohmann::json result = nlohmann::json::object();
        nlohmann::json readingsJsonArray = nlohmann::json::array();
        for (const auto &row: dbResponse.value().at(jp::ROWS)) {
            try {
                nlohmann::json reading = {
                    {"device_id", deviceId.value()},
                    {"value", row["value_numeric"].is_null() ? row["value_text"] : row["value_numeric"]},
                    {"timestamp", row["timestamp"]},
                    {"metadata", row["metadata"]}
                };

                readingsJsonArray.push_back(reading);
            } catch (const std::exception &e) {
                Core::Instance().mpLogger->errorf(
                    "[CORE_ACTIONS] [GET_DEVICE_READINGS] Failed to parse device reading from database response: %s",
                    e.what());
            }
        }

        result[cct::DEVICE_READINGS] = readingsJsonArray;
        handlerResult.result = to_string(result);
        co_return handlerResult;
    }

    awaitOptApiResponse CoreActions::getLogs(cmdMetaPtr pCommandMetadata, jsonPtr pParams) {
        API::ApiResponse handlerResult;
        handlerResult.id = pCommandMetadata->command.commandId;

        const auto moduleId = ActionHelpers::requireModuleId(*pParams);
        if (!moduleId.has_value()) {
            handlerResult.error = moduleId.error();
            co_return handlerResult;
        }

        const auto limit = ActionHelpers::requireLimitArg(*pParams);
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

        const auto dbResponse = co_await ActionHelpers::queryDatabase(Constants::Methods::GET, std::move(dbQuery));
        if (!dbResponse.has_value()) {
            handlerResult.error = dbResponse.error();
            co_return handlerResult;
        }

        nlohmann::json result = nlohmann::json::object();
        nlohmann::json readingsJsonArray = nlohmann::json::array();

        for (const auto &row: dbResponse.value().at(jp::ROWS)) {
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

    awaitOptApiResponse CoreActions::getDebugCache(const cmdMetaPtr pCommandMetadata, jsonPtr pParams) {
        API::ApiResponse handlerResult;
        handlerResult.id = pCommandMetadata->command.commandId;

        nlohmann::json result = nlohmann::json::object();
        result["modules_cache"] = nlohmann::json::array();
        for (const auto &module: Core::Instance().configCache().getAllModules()) {
            result["modules_cache"].push_back(module.to_json());
        }
        result["devices_cache"] = nlohmann::json::array();
        for (const auto &device: Core::Instance().configCache().getAllDevices()) {
            result["devices_cache"].push_back(device.to_json());
        }
        result["readings_cache"] = nlohmann::json::array();
        for (const auto &reading: Core::Instance().readingsCache().getDebugAll()) {
            result["readings_cache"].push_back(reading.to_json());
        }

        handlerResult.result = to_string(result);
        co_return handlerResult;
    }

    awaitOptApiResponse CoreActions::deleteDatabaseValue(cmdMetaPtr pCommandMetadata, jsonPtr pParams) {
        Core::Instance().mpLogger->debugf("[CORE_ACTIONS] [DELETE_DB_VALUE] called");
        API::ApiResponse handlerResult;
        handlerResult.id = pCommandMetadata->command.commandId;

        const auto deleteType = pParams->at(jp::TYPE).get<std::string_view>();
        const bool isModule = deleteType == cct::MODULE;

        const std::string_view tableName = Constants::methodTypeToDbTable(deleteType);
        if (tableName.empty()) {
            handlerResult.error = API::ApiError(
                API::ErrorCodes::INVALID_PARAMS,
                errorCodeToString(API::ErrorCodes::INVALID_PARAMS),
                "Invalid delete type: could not translate to database table.");
            co_return handlerResult;
        }

        // Resolve entity ID
        uint entityId;
        if (isModule) {
            const auto moduleId = ActionHelpers::requireModuleId(*pParams);
            if (!moduleId.has_value()) {
                handlerResult.error = moduleId.error();
                co_return handlerResult;
            }
            entityId = moduleId.value();
        } else {
            const auto deviceId = ActionHelpers::resolveDeviceId(*pParams);
            if (!deviceId.has_value()) {
                handlerResult.error = deviceId.error();
                co_return handlerResult;
            }
            entityId = deviceId.value();
        }

        nlohmann::json dbQuery = {
            {jp::TABLE, tableName},
            {jp::WHERE, {{cdi::ID, entityId}}}
        };

        // With path - remove JSONB field value(s), generates UPDATE with #- postgresql operator in DB API
        // Without path - delete entire record
        if (pParams->contains(jp::PATH)) {
            const auto deletePath = ActionHelpers::requirePath(*pParams);
            if (!deletePath.has_value()) {
                handlerResult.error = deletePath.error();
                co_return handlerResult;
            }

            dbQuery[jp::COLUMNS] = nlohmann::json::array({deletePath.value()});
        }

        auto dbResponse = co_await ActionHelpers::queryDatabase(Constants::Methods::DELETE, std::move(dbQuery));
        if (!dbResponse.has_value()) {
            handlerResult.error = dbResponse.error();
            co_return handlerResult;
        }

        handlerResult.result = to_string(nlohmann::json::object({{jr::STATUS, cc::OK}}));
        co_return handlerResult;
    }

    awaitOptApiResponse CoreActions::getReadingValue(cmdMetaPtr pCommandMetadata, jsonPtr pParams) {
        API::ApiResponse handlerResult;
        handlerResult.id = pCommandMetadata->command.commandId;

        const auto deviceId = ActionHelpers::resolveDeviceId(*pParams);
        if (!deviceId.has_value()) {
            handlerResult.error = deviceId.error();
            co_return handlerResult;
        }

        const auto deviceOpt = Core::Instance().configCache().getDevice(deviceId.value());
        if (!deviceOpt.has_value()) {
            handlerResult.error = API::ApiError(
                API::ErrorCodes::NOT_FOUND,
                errorCodeToString(API::ErrorCodes::NOT_FOUND),
                "Device with [" + std::to_string(deviceId.value()) + "] ID not found in cache");
            co_return handlerResult;
        }

        const auto &device = deviceOpt.value();
        const uint moduleId = device.moduleId;
        const uint deviceLogicId = device.logicId;
        const std::string deviceType = device.type;

        auto type = pParams->at(jp::TYPE).get<std::string>();
        const bool forceRefresh = pParams->contains(jp::FORCE) &&
                                  pParams->at(jp::FORCE).is_boolean() &&
                                  pParams->at(jp::FORCE).get<bool>();

        // Try cache if not forced refresh
        if (!forceRefresh) {
            if (deviceId.has_value()) {
                if (const auto cached = Core::Instance().readingsCache().getFresh(deviceId.value())) {
                    handlerResult.result = cached->value.dump();
                    co_return handlerResult;
                }
            }
        }

        nlohmann::json mediatorArgs = nlohmann::json::array({deviceLogicId});

        if (type == cct::DEVICE_VALUE) {
            if (deviceType == Constants::DeviceTypes::ACTUATOR) type = cmt::ACTUATOR_STATE;
            else if (deviceType == Constants::DeviceTypes::SENSOR)
                type = forceRefresh
                           ? cmt::FORCE_READ_SENSOR_VALUE
                           : cmt::SENSOR_VALUE;
            else {
                handlerResult.error = API::ApiError(
                    API::ErrorCodes::INVALID_PARAMS,
                    errorCodeToString(API::ErrorCodes::NOT_FOUND),
                    "Device [" + std::to_string(deviceId.value()) + "] type is undefined for use with"
                    " 'device_value' use 'sensor_value' or 'actuator_state' when delegating to mediator instead");
                co_return handlerResult;
            }
        }

        // Delegate to mediator
        auto response = co_await MediatorActions::sendToModule(pCommandMetadata, moduleId, type, mediatorArgs,
                                                               API::InternalApi::MethodTypes::GET);

        if (response.result.has_value()) handlerResult.result = std::move(response.result);
        else handlerResult.error = std::move(response.error);

        co_return handlerResult;
    }

    awaitOptApiResponse CoreActions::getModuleInfo(cmdMetaPtr pCommandMetadata, jsonPtr pParams) {
        API::ApiResponse handlerResult;
        handlerResult.id = pCommandMetadata->command.commandId;

        const auto moduleId = ActionHelpers::requireModuleId(*pParams);
        if (!moduleId.has_value()) {
            handlerResult.error = moduleId.error();
            co_return handlerResult;
        }

        const auto type = pParams->at(jp::TYPE).get<std::string>();

        nlohmann::json mediatorArgs = nlohmann::json::array();
        if (pParams->contains(jp::ARGS) && pParams->at(jp::ARGS).is_array()) {
            mediatorArgs = pParams->at(jp::ARGS);
        }

        // Delegate to mediator
        auto response = co_await MediatorActions::sendToModule(pCommandMetadata, moduleId.value(), type, mediatorArgs,
                                                               API::InternalApi::MethodTypes::GET);

        if (response.result.has_value()) handlerResult.result = std::move(response.result);
        else handlerResult.error = std::move(response.error);

        co_return handlerResult;
    }

    awaitOptApiResponse CoreActions::getModuleLogs(cmdMetaPtr pCommandMetadata, jsonPtr pParams) {
        API::ApiResponse handlerResult;
        handlerResult.id = pCommandMetadata->command.commandId;

        const auto moduleId = ActionHelpers::requireModuleId(*pParams);
        if (!moduleId.has_value()) {
            handlerResult.error = moduleId.error();
            co_return handlerResult;
        }

        const auto limit = ActionHelpers::requireLimitArg(*pParams);
        if (!limit.has_value()) {
            handlerResult.error = limit.error();
            co_return handlerResult;
        }

        const auto type = pParams->at(jp::TYPE).get<std::string>();

        // Delegate to mediator
        auto response = co_await MediatorActions::sendToModule(pCommandMetadata, moduleId.value(), type,
                                                               pParams->at(jp::ARGS),
                                                               API::InternalApi::MethodTypes::GET);

        if (response.result.has_value()) handlerResult.result = std::move(response.result);
        else handlerResult.error = std::move(response.error);

        co_return handlerResult;
    }


    // Set type handlers

    awaitOptApiResponse CoreActions::setConnectionType(const cmdMetaPtr pCommandMetadata, const jsonPtr pParams) {
        API::ApiResponse handlerResult;
        handlerResult.id = pCommandMetadata->command.commandId;

        if (!pParams->contains(jp::VALUE) || !pParams->at(jp::VALUE).is_string()) {
            handlerResult.error = API::ApiError(
                API::ErrorCodes::INVALID_PARAMS,
                errorCodeToString(API::ErrorCodes::INVALID_PARAMS),
                "Set connection requires 'value' parameter with connection type.");
            co_return handlerResult;
        }

        const auto setValue = pParams->at(jp::VALUE).get<std::string_view>();

        clearStaleConnectionTypes();

        connectionId_t connectionId;
        // Get Connection ID
        try {
            std::scoped_lock lock(Actions::msActiveRequestsLock);
            connectionId = Actions::msActiveRequests.at(pCommandMetadata->requestId).request.connectionId;
        } catch (...) {
            handlerResult.error = API::ApiError(
                API::ErrorCodes::INTERNAL_ERROR,
                errorCodeToString(API::ErrorCodes::INTERNAL_ERROR),
                "Could not find current connection ID");
            co_return handlerResult;
        }

        // Set connection type
        {
            const auto connectionType = sai::Target(setValue).type;
            std::scoped_lock lock(Actions::msConnectionsMapLock, Actions::msConnectionTypeMapLock);

            Actions::msConnectionsMap[connectionId] = connectionType;
            Actions::msConnectionTypeMap[connectionType].insert(connectionId);
        }

        const auto resultJson = nlohmann::json::object({{jr::STATUS, cc::OK}});
        handlerResult.result = to_string(resultJson);
        co_return handlerResult;
    }

    awaitOptApiResponse CoreActions::setDatabaseValue(cmdMetaPtr pCommandMetadata, jsonPtr pParams) {
        Core::Instance().mpLogger->debugf("[CORE_ACTIONS] [SET_DB_VALUE] called");
        API::ApiResponse handlerResult;
        handlerResult.id = pCommandMetadata->command.commandId;

        const auto setType = pParams->at(jp::TYPE).get<std::string_view>();

        // If VALUES parameter is present create new db record
        if (pParams->contains(jp::VALUES)) {
            nlohmann::json dbQuery;

            std::string_view tableName = Constants::methodTypeToDbTable(setType);
            if (tableName.empty()) {
                handlerResult.error = API::ApiError(
                    API::ErrorCodes::INVALID_PARAMS,
                    errorCodeToString(API::ErrorCodes::INVALID_PARAMS),
                    "Invalid set type: could not translate set type to database table.");
                co_return handlerResult;
            }

            dbQuery[jp::TABLE] = tableName;
            dbQuery[jp::VALUES] = pParams->at(jp::VALUES);

            // Optional param
            if (pParams->contains(jp::RETURNING)) dbQuery[jp::RETURNING] = pParams->at(jp::RETURNING);

            auto dbResponse = co_await ActionHelpers::queryDatabase(Constants::Methods::SET, std::move(dbQuery));
            if (!dbResponse.has_value()) {
                handlerResult.error = dbResponse.error();
                co_return handlerResult;
            }

            nlohmann::json setResult;
            setResult[jr::STATUS] = cc::OK;
            setResult.merge_patch(dbResponse.value());

            handlerResult.result = to_string(setResult);
            co_return handlerResult;
        }

        // Updating existing db record requires mode, path and value params
        auto setMode = ActionHelpers::requireMode(*pParams);
        if (!setMode.has_value()) {
            handlerResult.error = setMode.error();
            co_return handlerResult;
        }

        auto setPathKeys = ActionHelpers::requirePath(*pParams);
        if (!setPathKeys.has_value()) {
            handlerResult.error = setPathKeys.error();
            co_return handlerResult;
        }

        auto setValue = ActionHelpers::requireValue(*pParams);
        if (!setValue.has_value()) {
            handlerResult.error = setValue.error();
            co_return handlerResult;
        }


        std::expected<std::string, API::ApiError> result;

        if (setType == cct::MODULE) {
            result = co_await updateModule(pParams, std::move(setMode.value()), std::move(setPathKeys.value()),
                                           std::move(setValue.value()));
        } else if (setType == cct::DEVICE) {
            result = co_await updateDevice(pParams, std::move(setMode.value()), std::move(setPathKeys.value()),
                                           std::move(setValue.value()));
        } else {
            handlerResult.error = API::ApiError(
                API::ErrorCodes::INTERNAL_ERROR,
                errorCodeToString(API::ErrorCodes::INTERNAL_ERROR),
                "Set type ("s + setType.data() + ") is not supporting updates.");
            co_return handlerResult;
        }

        if (!result.has_value()) {
            handlerResult.error = result.error();
            co_return handlerResult;
        }

        nlohmann::json responseResult;
        responseResult[jr::STATUS] = cc::OK;

        if (!result.value().empty() && nlohmann::json::accept(result.value())) {
            nlohmann::json resultJson;
            try {
                resultJson = nlohmann::json::parse(result.value());
            } catch (const std::exception &e) {
                handlerResult.error = API::ApiError(
                    API::ErrorCodes::INTERNAL_ERROR,
                    errorCodeToString(API::ErrorCodes::INTERNAL_ERROR),
                    "Parsing JSON result failed: "s + e.what());
                co_return handlerResult;
            }

            responseResult.merge_patch(resultJson);
        }

        handlerResult.result = to_string(responseResult);
        co_return handlerResult;
    }

    awaitOptApiResponse CoreActions::setModuleValue(cmdMetaPtr pCommandMetadata, jsonPtr pParams) {
        API::ApiResponse handlerResult;
        handlerResult.id = pCommandMetadata->command.commandId;

        const auto moduleId = ActionHelpers::requireModuleId(*pParams);
        if (!moduleId.has_value()) {
            handlerResult.error = moduleId.error();
            co_return handlerResult;
        }

        const auto deviceId = ActionHelpers::resolveDeviceId(*pParams);
        if (!deviceId.has_value()) {
            handlerResult.error = deviceId.error();
            co_return handlerResult;
        }

        const auto mediatorArgs = pParams->at(jp::ARGS).get<nlohmann::json>();
        const auto type = pParams->at(jp::TYPE).get<std::string_view>();

        // Delegate to mediator
        auto response = co_await MediatorActions::sendToModule(pCommandMetadata, moduleId.value(), type, mediatorArgs,
                                                               API::InternalApi::MethodTypes::SET);

        if (response.result.has_value()) {
            if (nlohmann::json::accept(response.result.value())) {
                nlohmann::json responseJson;

                try {
                    responseJson = nlohmann::json::parse(response.result.value());
                } catch (const std::exception &e) {
                    handlerResult.error = API::ApiError(
                        API::ErrorCodes::INTERNAL_ERROR,
                        errorCodeToString(API::ErrorCodes::INTERNAL_ERROR),
                        "Faield to parse JSON response: "s + e.what());
                    co_return handlerResult;
                }
                responseJson[jr::STATUS] = cc::OK;

                handlerResult.result = to_string(responseJson);
            } else {
                handlerResult.result = std::move(response.result);
            }
        } else handlerResult.error = std::move(response.error);

        co_return handlerResult;
    }


    // Command handler-type registers

    std::unordered_map<std::string_view, CoreActions::coreCommandHandler> CoreActions::msGetTypeHandlersRegistry{
        // Core types
        {cct::MODULES, getModules},
        {cct::MODULE, getModule},
        {cct::MODULE_DEVICES, getModuleDevices},
        {cct::DEVICES, getDevices},
        {cct::DEVICE, getDevice},
        // Debug types
        {cct::_DEBUG_CACHE, getDebugCache},
        // Core types delegated to mediator actions
        {cct::DEVICE_VALUE, getReadingValue},
        // Core types delegated to database actions
        {cct::DEVICE_READINGS, getDeviceReadings},
        {cct::LOGS, getLogs},
        // Mediator types
        {cmt::ACTUATOR_STATE, getReadingValue},
        {cmt::SENSOR_VALUE, getReadingValue},
        {cmt::FORCE_READ_SENSOR_VALUE, getReadingValue},
        {cmt::BATTERY_LEVEL, getModuleInfo},
        {cmt::DEVICE_LIST, getModuleInfo},
        {cmt::MODULE_LOGS, getModuleLogs},
    };

    std::unordered_map<std::string_view, CoreActions::coreCommandHandler> CoreActions::msSetTypeHandlersRegistry{
        // Core types
        {cct::CONNECTION_TYPE, setConnectionType},
        // Core types delegated to database actions
        {cct::MODULE, setDatabaseValue},
        {cct::DEVICE, setDatabaseValue},
        // Mediator types
        {cmt::CONFIG_OPTION, ActionHelpers::delegateToMediator},
        {cmt::TOGGLE_ACTUATOR, setModuleValue},
        {cmt::SET_ACTUATOR_VALUE, setModuleValue},
    };


    // Utility

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

    ba::awaitable<std::expected<std::string, API::ApiError> > CoreActions::updateModule(const jsonPtr pParams,
        std::string &&mode,
        std::string &&path,
        nlohmann::json &&value) {
        Core::Instance().mpLogger->debugf("[CORE_ACTIONS] [UPDATE_MODULE] called");

        const auto moduleId = ActionHelpers::requireModuleId(*pParams);
        if (!moduleId.has_value())
            co_return std::unexpected(moduleId.error());

        // Check in cache if module exists
        if (!Core::Instance().configCache().getModule(moduleId.value()))
            co_return std::unexpected(API::ApiError(
                API::ErrorCodes::INVALID_PARAMS,
                errorCodeToString(API::ErrorCodes::INVALID_PARAMS),
                "Could not set module value: module with ID ["s + std::to_string(moduleId.value()) + "] not found"));

        EntityUpdateContext ctx{
            .pParams = pParams,
            .tableName = cdi::MODULES,
            .entityName = cct::MODULE,
            .id = moduleId.value(),
            .getCache = [id = moduleId.value()]-> std::optional<CachedEntity> {
                return Core::Instance().configCache().getModule(id);
            },
            .compareExchangeFresh = [id = moduleId.value()](const bool expected, const bool desired) {
                return Core::Instance().configCache().compareExchangeIsModuleFresh(id, expected, desired);
            },
            .refreshCache = [] -> ba::awaitable<void> {
                co_await DatabaseActions::fetchModulesConfigs();
            }
        };

        co_return co_await updateEntity(std::move(ctx), std::move(mode), std::move(path), std::move(value));
    }

    ba::awaitable<std::expected<std::string, API::ApiError> > CoreActions::updateDevice(const jsonPtr pParams,
        std::string &&mode,
        std::string &&path,
        nlohmann::json &&value) {
        Core::Instance().mpLogger->debugf("[CORE_ACTIONS] [UPDATE_DEVICE] called");

        const auto deviceId = ActionHelpers::resolveDeviceId(*pParams);
        if (!deviceId)
            co_return std::unexpected(deviceId.error());

        // Check in cache if device exists
        if (!Core::Instance().configCache().getDevice(deviceId.value()))
            co_return std::unexpected(API::ApiError(
                API::ErrorCodes::INVALID_PARAMS,
                errorCodeToString(API::ErrorCodes::INVALID_PARAMS),
                "Could not set device value: device with ID ["s + std::to_string(deviceId.value()) + "] not found"));

        EntityUpdateContext ctx{
            .pParams = pParams,
            .tableName = cdi::DEVICES,
            .entityName = cct::DEVICE,
            .id = deviceId.value(),
            .getCache = [id = deviceId.value()] -> std::optional<CachedEntity> {
                return Core::Instance().configCache().getDevice(id);
            },
            .compareExchangeFresh = [id = deviceId.value()](const bool expected, const bool desired) {
                return Core::Instance().configCache().compareExchangeIsDeviceFresh(id, expected, desired);
            },
            .refreshCache = []() -> ba::awaitable<void> {
                co_await DatabaseActions::fetchDevicesConfigs();
            }
        };

        co_return co_await updateEntity(std::move(ctx), std::move(mode), std::move(path), std::move(value));
    }

    std::optional<nlohmann::json> CoreActions::resolveJsonbField(CachedEntity &entity, std::string_view fieldName) {
        return std::visit([fieldName](const auto &e) -> std::optional<nlohmann::json> {
            if (fieldName == cdi::CONFIG) return e.config;
            return std::nullopt;
        }, entity);
    }

    ba::awaitable<std::expected<CoreActions::CachedEntity, API::ApiError> > CoreActions::resolveEntityCache(
        const EntityUpdateContext &ctx) {
        bool constexpr expected = true, desired = false;

        // Return cache if it's fresh
        if (ctx.compareExchangeFresh(expected, desired).value_or(false)) {
            auto cache = ctx.getCache();
            if (!cache)
                co_return std::unexpected(API::ApiError(
                    API::ErrorCodes::INTERNAL_ERROR,
                    errorCodeToString(API::ErrorCodes::INTERNAL_ERROR),
                    "Could not retrieve "s + ctx.entityName.data() + " cache"));
            co_return std::move(*cache);
        }

        // Refresh cache if it's stale
        co_await ctx.refreshCache();

        // Check freshness again after refreshing cache
        if (!ctx.compareExchangeFresh(expected, desired).value_or(false))
            co_return std::unexpected(API::ApiError(
                API::ErrorCodes::INTERNAL_ERROR,
                errorCodeToString(API::ErrorCodes::INTERNAL_ERROR),
                "Could not set "s + ctx.entityName.data() +
                " value: cache is stale and failed to update"));

        auto cache = ctx.getCache();
        if (!cache)
            co_return std::unexpected(API::ApiError(
                API::ErrorCodes::INTERNAL_ERROR,
                errorCodeToString(API::ErrorCodes::INTERNAL_ERROR),
                "Could not retrieve "s + ctx.entityName.data() + " cache after refresh"));

        co_return std::move(*cache);
    }

    ba::awaitable<std::expected<std::string, API::ApiError> > CoreActions::updateEntity(EntityUpdateContext &&ctx,
        std::string &&mode,
        std::string &&path,
        nlohmann::json &&value) {
        auto entity = co_await resolveEntityCache(ctx);
        if (!entity)
            co_return std::unexpected(entity.error());

        nlohmann::json dbQuery = {{jp::TABLE, ctx.tableName}};

        // Optional param
        if (ctx.pParams->contains(jp::RETURNING)) dbQuery[jp::RETURNING] = ctx.pParams->at(jp::RETURNING);

        // Append mode is used for jsonb type fields
        // Append cached value with value param, set appended value in farther replace segment
        if (mode == co::APPEND) {
            std::vector<std::string_view> keys;
            // Convert dot separated path to slash separated for later use in creating json_pointer
            std::string jsonPath = boost::algorithm::replace_all_copy(path, ".", "/");
            boost::split(keys, jsonPath, boost::is_any_of("/"));

            // Handle jsonb fields
            auto field = resolveJsonbField(entity.value(), keys.front());
            if (!field.has_value())
                co_return std::unexpected(API::ApiError(
                    API::ErrorCodes::INVALID_PARAMS,
                    errorCodeToString(API::ErrorCodes::INVALID_PARAMS),
                    "Could not set "s + ctx.entityName.data() +
                    " value: path does not point to JSONB type field or it is not supported"));

            // convert config/<json_path> to /<json_path>
            jsonPath = jsonPath.substr(jsonPath.find('/'));

            nlohmann::json::json_pointer ptr;
            try {
                ptr = nlohmann::json::json_pointer(jsonPath);
            } catch (const std::exception &e) {
                co_return std::unexpected(API::ApiError(
                    API::ErrorCodes::INVALID_PARAMS,
                    errorCodeToString(API::ErrorCodes::INVALID_PARAMS),
                    "Could not set "s + ctx.entityName.data() + " value: "s + e.what()));
            }

            auto &node = field.value()[ptr];

            if (node.is_null()) {
                co_return std::unexpected(API::ApiError(
                    API::ErrorCodes::INVALID_PARAMS,
                    errorCodeToString(API::ErrorCodes::INVALID_PARAMS),
                    "Could not set "s + ctx.entityName.data() + " value: can not append to non existing field use "s +
                    co::OVERWRITE.data() + " mode instead"));
            }

            if (node.is_array()) {
                node.push_back(value);
            } else if (node.is_object()) {
                node.merge_patch(value);
            } else {
                co_return std::unexpected(API::ApiError(
                    API::ErrorCodes::INVALID_PARAMS,
                    errorCodeToString(API::ErrorCodes::INVALID_PARAMS),
                    "Could not set "s + ctx.entityName.data() +
                    " value: append is meant for merging objects and appending arrays, "
                    "it does not support primitive types"));
            }

            // Overwrite value with appended JSON node
            value = node;
        }

        dbQuery.update({
            {jp::VALUES, {{path, value}}},
            {jp::WHERE, {{cdi::ID, ctx.id}}}
        });

        auto dbResponse = co_await ActionHelpers::queryDatabase(Constants::Methods::SET, std::move(dbQuery));
        if (!dbResponse)
            co_return std::unexpected(dbResponse.error());

        co_return to_string(dbResponse.value());
    }
}
