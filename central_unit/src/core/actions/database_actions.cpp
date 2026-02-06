#include "database_actions.h"

namespace SmartHome {
    using ai = API::InternalApi;

    namespace jmik = JsonRpcStrings::ModuleInfoKeys;

    awaitOptApiResponse DatabaseActions::databaseRequestHandler(const cmdMetaPtr &commandMetadata) {
        Core::Instance().mpLogger->debug("[DATABASE_ACTIONS] [REQUEST_HANDLER] called");
        const auto &command = commandMetadata->command;


        API::ApiResponse commandResult;
        API::ApiError error;

        commandResult.id = command.commandId;
        error.code = API::ErrorCodes::INVALID_PARAMS;
        error.message = API::errorCodeToString(error.code);

        if (!areParamsValid(error, command)) {
            commandResult.error = error;
            co_return commandResult;
        }

        API::ApiRequest requestToDatabase;

        prepareRequestToDatabase(requestToDatabase, command);

        commandResult = co_await sendRequestToDbService(std::move(requestToDatabase), commandMetadata);

        co_return commandResult;
    }

    ba::awaitable<nlohmann::json> DatabaseActions::getModuleAddressingInfo(uint moduleId) {
        API::ApiRequest request;

        nlohmann::json dbQuery = {
            {"table", "modules"},
            {"columns", {"logic_address", "config"}},
            {"where", {{"id", moduleId}}},
        };

        const API::InternalApi::Method method(API::InternalApi::MethodTypes::GET);

        request.method = API::getTargetMethodString(ai::Target(ai::TargetTypes::DATABASE).to_string(),
                                                    method.to_string());
        request.id = Actions::getNextId();

        request.params = dbQuery;

        API::InternalApi::Command command(request);
        auto pCmdMeta = std::make_shared<Actions::CommandMetadata>(
            command,
            std::make_shared<ba::steady_timer>(Core::Instance().coreUtilityIoContext()),
            Actions::getNextId());


        auto response = co_await sendRequestToDbService(std::move(request), pCmdMeta);

        nlohmann::json result;

        if (response.error.has_value()) {
            result["error"] = response.error.value().data;
            co_return result;
        }

        if (response.result.has_value()) {
            auto responseResultJson = nlohmann::json::parse(response.result.value());

            if (responseResultJson.contains("affected_rows") &&
                responseResultJson["affected_rows"] == 1 &&
                responseResultJson.contains("rows")) {
                auto row = responseResultJson["rows"].front();

                try {
                    result[jmik::LOGIC_ADDRESS] = row[jmik::LOGIC_ADDRESS];
                    result[jmik::RF_CHANNEL] = row["config"][jmik::RF_CHANNEL];
                } catch (const std::exception &e) {
                    Core::Instance().mpLogger->errorf(
                        "[DATABASE_ACTIONS] [GET_ADDR_INFO] Failed to parse db response %s", e.what());
                }
            } else {
                result["error"] = "Module not found";
            }
        }
        co_return result;
    }


    void DatabaseActions::postSensorReading(uint moduleId, uint sensorLogicId,
                                            nlohmann::json value,
                                            nlohmann::json metadata) {
        API::ApiRequest notification;


        nlohmann::json dbQuerySubselect = {
            {"table", "sensors"},
            {"columns", {"id"}},
            {
                "where", {
                    {"module_id", moduleId},
                    {"logic_id", sensorLogicId}
                }
            }
        };

        nlohmann::json dbQuery = {
            {"table", "sensor_readings"},
            {
                "values", {
                    {"sensor_id", {{"$subselect", dbQuerySubselect}}},
                    {value.is_number() ? "value_numeric" : "value_text", value},
                    {"metadata", metadata}
                }
            },
        };

        const API::InternalApi::Method method(API::InternalApi::MethodTypes::SET);
        notification.method = API::getTargetMethodString(ai::Target(ai::TargetTypes::DATABASE).to_string(),
                                                         method.to_string());

        notification.params = dbQuery;

        sendNotificationToDbService(std::move(notification));
    }

    void DatabaseActions::postLog(uint moduleId, std::string_view type, std::string_view content) {
        API::ApiRequest notification;


        nlohmann::json dbQuery = {
            {"table", "logs"},
            {
                "values", {
                    {"type", type},
                    {"content", content},
                    {"module_id", moduleId}
                }
            },
        };

        const API::InternalApi::Method method(API::InternalApi::MethodTypes::SET);
        notification.method = API::getTargetMethodString(ai::Target(ai::TargetTypes::DATABASE).to_string(),
                                                         method.to_string());

        notification.params = dbQuery;

        sendNotificationToDbService(std::move(notification));
    }

    void DatabaseActions::updateModuleLastOnline(uint moduleId) {
        API::ApiRequest notification;

        auto nowTimeP = std::chrono::system_clock::now();
        auto nowTimeT = std::chrono::system_clock::to_time_t(nowTimeP);

        std::tm tmUTC;
        gmtime_r(&nowTimeT, &tmUTC);

        std::ostringstream oss;
        oss << std::put_time(&tmUTC, "%Y-%m-%dT%H:%M:%SZ");

        nlohmann::json dbQuery = {
            {"table", "modules"},
            {"values", {{"last_online", oss.str()}}},
            {"where", {{"id", moduleId}}}
        };

        const API::InternalApi::Method method(API::InternalApi::MethodTypes::SET);
        notification.method = API::getTargetMethodString(ai::Target(ai::TargetTypes::DATABASE).to_string(),
                                                         method.to_string());

        notification.params = dbQuery;

        sendNotificationToDbService(std::move(notification));
    }

    void DatabaseActions::sendNotificationToDbService(API::ApiRequest &&notification) {
        connectionId_t dbServiceConnectionId;
        bool foundDbServiceConnection = false;

        // Find db-service connection
        {
            std::scoped_lock lock(Actions::msConnectionTypeMapLock);

            const auto iter = Actions::msConnectionTypeMap.find(sai::TargetTypes::DATABASE);
            if (iter != Actions::msConnectionTypeMap.end() && !iter->second.empty()) {
                dbServiceConnectionId = *iter->second.begin();
                foundDbServiceConnection = true;
            }
        }

        if (!foundDbServiceConnection) {
            Core::Instance().mpLogger->error(
                "[DATABASE_ACTIONS] [SEND_NOTIFY] Failed to send notification to database: connection not found");
            return;
        }

        ba::post(Core::Instance().coreWorkerIoContext(), [notification, dbServiceConnectionId]()mutable {
            Actions::handleOutgoingRequest(dbServiceConnectionId, std::move(notification), nullptr);
        });
    }


    ba::awaitable<API::ApiResponse> DatabaseActions::sendRequestToDbService(API::ApiRequest &&request,
                                                                            cmdMetaPtr commandMetadata) {
        auto promise = std::make_shared<std::promise<API::ApiResponse> >();
        auto future = promise->get_future();

        API::ApiResponse requestResult;
        requestResult.id = commandMetadata->command.commandId;


        connectionId_t dbServiceConnectionId;
        bool foundDbServiceConnection = false;

        // Find db-service connection
        {
            std::scoped_lock lock(Actions::msConnectionTypeMapLock);

            auto iter = Actions::msConnectionTypeMap.find(sai::TargetTypes::DATABASE);
            if (iter != Actions::msConnectionTypeMap.end() && !iter->second.empty()) {
                dbServiceConnectionId = *iter->second.begin();
                foundDbServiceConnection = true;
            }
        }

        if (!foundDbServiceConnection) {
            requestResult.error = API::ApiError(
                API::ErrorCodes::INTERNAL_ERROR,
                API::errorCodeToString(API::ErrorCodes::INTERNAL_ERROR),
                "Could not find db-service connection");
            co_return requestResult;
        }

        ba::post(Core::Instance().coreWorkerIoContext(),
                 [promise, request, dbServiceConnectionId]()mutable {
                     Actions::handleOutgoingRequest(dbServiceConnectionId, std::move(request), promise);
                 });

        // TODO Consider using condition_variable
        while (commandMetadata->isPending() && future.wait_for(0ms) != std::future_status::ready) {
            co_await ba::steady_timer(co_await ba::this_coro::executor, 10ms).async_wait(ba::use_awaitable);
        }

        try {
            API::ApiResponse response(future.get());
            if (response.result.has_value()) requestResult.result = std::move(response.result);
            else if (response.error.has_value()) requestResult.error = std::move(response.error);
            else {
                throw std::runtime_error("Received an invalid response: no result or error");
            }
        } catch (std::exception &e) {
            requestResult.error = API::ApiError(
                API::ErrorCodes::INTERNAL_ERROR,
                API::errorCodeToString(API::ErrorCodes::INTERNAL_ERROR),
                e.what());
        }

        co_return requestResult;
    }

    bool DatabaseActions::areParamsValid(API::ApiError &error, const API::InternalApi::Command &command) {
        if (!command.params.has_value()) {
            error.data = "No parameters";
            return false;
        }

        const auto &params = command.params.value();
        if (!params.is_object() || params.empty()) {
            error.data = "Parameters must be a non-empty object";
            return false;
        }

        return true;
    }

    void DatabaseActions::prepareRequestToDatabase(API::ApiRequest &request,
                                                   const API::InternalApi::Command &command) {
        request.id = command.commandId;
        request.method = API::getTargetMethodString(ai::Target(ai::TargetTypes::DATABASE).to_string(),
                                                    command.method.to_string());

        request.params = command.params;
    }
}
