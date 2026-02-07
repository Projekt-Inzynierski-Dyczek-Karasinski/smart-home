#include "core_actions.h"

#include <boost/algorithm/string/case_conv.hpp>

#include "database_actions.h"

namespace SmartHome {
    namespace jp = JsonRpcStrings::ParamsKeys;

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
        commandResult.id = command.commandId;

        // TODO rework core get handler to use database and cached values

        // TODO implement core cached data object with getter methods
        // TODO remove - temporary code for API testing
        static const auto tmpDataGetter = [](const std::string_view value) {
            const std::map<std::string_view, std::string> tmpMap = {
                {
                    "modules_status",
                    R"([{"id":"1","name":"Lightning","description":"LED lights","values":[{"name":"Brightness","value":"75%"},{"name":"Temperature","value":"3200K"}],"actions":[{"name":"Turn On","method":"turnOn"},{"name":"Turn Off","method":"turnOff"}]},{"id":"2","name":"Thermometer","description":"Thermometer + Hygrometer","values":[{"name":"Temperature","value":"22C"},{"name":"Humidity","value":"44%"}],"actions":[{"name":"Turn On","method":"turnOn"},{"name":"Turn Off","method":"turnOff"},{"name":"Update Values","method":"updateValues"}]},{"id":"1","name":"Lightning","description":"LED lights","values":[{"name":"Brightness","value":"75%"},{"name":"Temperature","value":"3200K"}],"actions":[{"name":"Turn On","method":"turnOn"},{"name":"Turn Off","method":"turnOff"}]},{"id":"1","name":"Lightning","description":"LED lights","values":[{"name":"Brightness","value":"75%"},{"name":"Temperature","value":"3200K"}],"actions":[{"name":"Turn On","method":"turnOn"},{"name":"Turn Off","method":"turnOff"}]},{"id":"1","name":"Lightning","description":"LED lights","values":[{"name":"Brightness","value":"75%"},{"name":"Temperature","value":"3200K"}],"actions":[{"name":"Turn On","method":"turnOn"},{"name":"Turn Off","method":"turnOff"}]},{"id":"1","name":"Lightning","description":"LED lights","values":[{"name":"Brightness","value":"75%"},{"name":"Temperature","value":"3200K"}],"actions":[{"name":"Turn On","method":"turnOn"},{"name":"Turn Off","method":"turnOff"}]},{"id":"1","name":"Lightning","description":"LED lights","values":[{"name":"Brightness","value":"75%"},{"name":"Temperature","value":"3200K"}],"actions":[{"name":"Turn On","method":"turnOn"},{"name":"Turn Off","method":"turnOff"}]},{"id":"1","name":"Lightning","description":"LED lights","values":[{"name":"Brightness","value":"75%"},{"name":"Temperature","value":"3200K"}],"actions":[{"name":"Turn On","method":"turnOn"},{"name":"Turn Off","method":"turnOff"}]},{"id":"1","name":"Lightning","description":"LED lights","values":[{"name":"Brightness","value":"75%"},{"name":"Temperature","value":"3200K"}],"actions":[{"name":"Turn On","method":"turnOn"},{"name":"Turn Off","method":"turnOff"}]},{"id":"1","name":"Lightning","description":"LED lights","values":[{"name":"Brightness","value":"75%"},{"name":"Temperature","value":"3200K"}],"actions":[{"name":"Turn On","method":"turnOn"},{"name":"Turn Off","method":"turnOff"}]}])"
                },
                {
                    "module_status",
                    R"({"Id":22,"Status":"Online","Battery":"56%","Logs":["Received status request","Woke from sleep - incoming traffic","Going to sleep for 30000000ms","Sent requested value","Received read value request"]})"
                }
            };

            const auto iter = tmpMap.find(boost::algorithm::to_lower_copy(std::string(value)));
            std::string result = (iter != tmpMap.end()) ? iter->second.data() : "";

            return result;
        };


        std::string queryResponse;

        bool invalidParamsFlag = false;
        if (command.params.has_value()) {
            const auto &params = command.params.value();
            size_t paramsSize = 0;

            // params must be an array
            if (params.is_array()) {
                paramsSize = params.size();
            } else {
                invalidParamsFlag = true;
            }

            // params must have at least a query target
            if (!invalidParamsFlag && paramsSize > 0 && params[0].is_string()) {
                // TODO pass query target into target resolver
                queryResponse = tmpDataGetter(params[0].get<std::string>());
                // TODO remove - temporary code for API testing
                commandResult.result = queryResponse;
            } else {
                invalidParamsFlag = true;
            }

            // params might have query conditions object
            if (!invalidParamsFlag && paramsSize > 1 && params[1].is_object()) {
                nlohmann::json conditionParam = params[1];
                // TODO implement condition handler

                // TODO remove - temporary code for API testing
                auto index = conditionParam.find("id");
                if (index != conditionParam.end()) {
                    if (conditionParam["id"] == 22) {
                        nlohmann::json response;
                        response["id"] = 22;
                        response["object"] = queryResponse;
                        commandResult.result = nlohmann::to_string(response);
                    }
                }
            }

            //TODO pass query and condition to coreCachedDataGetter
        }

        if (invalidParamsFlag || !commandResult.result.has_value()) {
            commandResult.error = API::ApiError(
                API::ErrorCodes::INVALID_PARAMS,
                API::errorCodeToString(API::ErrorCodes::INVALID_PARAMS).data(),
                "Query failed"
            );
        }

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
            case CoreActions::SetKeys::CONNECTION_TYPE:
                if (CoreActions::setConnectionType(commandMetadata, value)) break;
                error.code = API::ErrorCodes::INTERNAL_ERROR;
                error.message = API::errorCodeToString(error.code);
                error.data = "Failed to set connection type";
                commandResult.error = error;
                co_return commandResult;
            case CoreActions::SetKeys::UNDEFINED:
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
        if (typeParam == "modules_changed") {
            co_await DatabaseActions::fetchModulesConfigs();
            co_return std::nullopt;
        }

        if (typeParam == "sensors_changed") {
            co_await DatabaseActions::fetchSensorsConfigs();
            co_return std::nullopt;
        }

        // Module mediator notifications
        if (typeParam == "manual_trigger") {
            // Handle manually trigger notification from module
        }


        // TODO !pr finish, implement common constant strings

        if (typeParam == "")

            co_return std::nullopt;
    }

    std::string_view CoreActions::setKeyToString(const SetKeys setKey) {
        switch (setKey) {
            case SetKeys::CONNECTION_TYPE:
                return msCONNECTION_TYPE_STRING;
            case SetKeys::UNDEFINED:
            default:
                return "undefined";
        }
    }

    CoreActions::SetKeys CoreActions::stringToSetKey(const std::string_view setKey) {
        std::map<std::string_view, SetKeys> setKeyMap = {
            {msCONNECTION_TYPE_STRING, SetKeys::CONNECTION_TYPE}
        };

        const auto iter = setKeyMap.find(boost::algorithm::to_lower_copy(std::string(setKey)));
        return (iter != setKeyMap.end()) ? iter->second : SetKeys::UNDEFINED;
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
}
