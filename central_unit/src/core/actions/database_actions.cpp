#include "database_actions.h"
#include "utils.h"

namespace SmartHome {
    using ai = API::InternalApi;

    namespace jmik = JsonRpcStrings::ModuleInfoKeys;
    namespace jp = JsonRpcStrings::ParamsKeys;

    awaitOptApiResponse DatabaseActions::databaseRequestHandler(cmdMetaPtr commandMetadata) {
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
            {jp::TABLE, "modules"},
            {jp::COLUMNS, {"logic_address", "name", "config", "last_online"}},
            {jp::WHERE, {{"id", moduleId}}},
        };

        const API::InternalApi::Method method(API::InternalApi::MethodTypes::GET);

        request.method = API::getTargetMethodString(ai::Target(ai::TargetTypes::DATABASE).to_string(),
                                                    method.to_string());
        request.id = Actions::getNextId();

        request.params = dbQuery;
        auto response = co_await sendRequestToDbService(std::move(request));

        nlohmann::json result;

        if (response.error.has_value()) {
            result[jp::ERROR] = response.error.value().data;
            co_return result;
        }

        if (response.result.has_value()) {
            auto responseResultJson = nlohmann::json::parse(response.result.value());

            if (responseResultJson.contains(jp::AFFECTED_ROWS) &&
                responseResultJson[jp::AFFECTED_ROWS] == 1 &&
                responseResultJson.contains(jp::ROWS)) {
                auto row = responseResultJson[jp::ROWS].front();

                // Extract logic address and RF channel from response
                try {
                    result[jmik::LOGIC_ADDRESS] = row[jmik::LOGIC_ADDRESS];
                    result[Constants::ModuleConfigKeys::CONNECTION] =
                            row["config"][Constants::ModuleConfigKeys::CONNECTION];
                } catch (const std::exception &e) {
                    Core::Instance().mpLogger->errorf(
                        "[DATABASE_ACTIONS] [GET_ADDR_INFO] Failed to parse db response %s", e.what());
                }

                // Update config cache with retrieved info
                try {
                    auto module = CachedModule{
                        .id = moduleId,
                        .logicAddress = row[jmik::LOGIC_ADDRESS],
                        .name = row["name"],
                        .config = row["config"],
                    };

                    if (!row["last_online"].is_null()) {
                        module.lastOnline = Utils::parseTimestampTz(row["last_online"]);
                    }

                    Core::Instance().configCache().setModule(module);
                } catch (const std::exception &e) {
                    Core::Instance().mpLogger->errorf(
                        "[DATABASE_ACTIONS] [GET_ADDR_INFO] Failed to update config cache: %s", e.what());
                }
            } else {
                result[jp::ERROR] = "Module not found";
            }
        }
        co_return result;
    }


    void DatabaseActions::postDeviceReading(
        uint deviceId,
        nlohmann::json value, nlohmann::json metadata) {
        API::ApiRequest notification;

        nlohmann::json dbQuery = {
            {jp::TABLE, "device_readings"},
            {
                jp::VALUES, {
                    {"device_id", deviceId},
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
            {jp::TABLE, "logs"},
            {
                jp::VALUES, {
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

        const auto timestamp = Utils::timePointToTimestampTz(std::chrono::system_clock::now());

        nlohmann::json dbQuery = {
            {jp::TABLE, "modules"},
            {jp::VALUES, {{"last_online", timestamp}}},
            {jp::WHERE, {{"id", moduleId}}}
        };

        const API::InternalApi::Method method(API::InternalApi::MethodTypes::SET);
        notification.method = API::getTargetMethodString(ai::Target(ai::TargetTypes::DATABASE).to_string(),
                                                         method.to_string());

        notification.params = dbQuery;

        sendNotificationToDbService(std::move(notification));
    }

    ba::awaitable<void> DatabaseActions::fetchModulesConfigs() {
        auto &cache = Core::Instance().configCache();
        cache.clearModules(); // Clear modules cache before fetching to avoid stale configs
        API::ApiRequest request;

        nlohmann::json dbQuery = {
            {jp::TABLE, "modules"},
            {jp::COLUMNS, nlohmann::json::array({"id", "name", "logic_address", "config", "last_online"})},
        };

        const API::InternalApi::Target target(API::InternalApi::TargetTypes::DATABASE);
        const API::InternalApi::Method method(API::InternalApi::MethodTypes::GET);
        request.method = API::getTargetMethodString(target.to_string(), method.to_string());

        request.params = dbQuery;
        request.id = Actions::getNextId();


        auto response = co_await sendRequestToDbService(std::move(request));

        if (response.error.has_value()) {
            Core::Instance().mpLogger->errorf(
                "[DATABASE_ACTIONS] [FETCH_MODULE_CONFIGS] Failed to fetch modules configs: %s",
                response.error.value().data.c_str());
            co_return;
        }

        if (response.result.has_value()) {
            auto responseResultJson = nlohmann::json::parse(response.result.value());

            if (responseResultJson.contains(jp::AFFECTED_ROWS) &&
                responseResultJson.contains(jp::ROWS)) {
                for (const auto &row: responseResultJson[jp::ROWS]) {
                    try {
                        CachedModule module{
                            .id = row["id"],
                            .logicAddress = row["logic_address"],
                            .name = row["name"],
                            .config = row["config"],
                        };

                        if (!row["last_online"].is_null()) {
                            module.lastOnline = Utils::parseTimestampTz(row["last_online"]);
                        }

                        cache.setModule(module);
                    } catch (const std::exception &e) {
                        Core::Instance().mpLogger->errorf(
                            "[DATABASE_ACTIONS] [FETCH_MODULE_CONFIGS] Failed to parse db response: %s", e.what());
                    }
                }
            } else {
                Core::Instance().mpLogger->error(
                    "[DATABASE_ACTIONS] [FETCH_MODULE_CONFIGS] Invalid db response format");
            }
        }
    }

    ba::awaitable<void> DatabaseActions::fetchDevicesConfigs() {
        // Clear readings cache to avoid stale readings after device config changes
        Core::Instance().readingsCache().clear();

        auto &cache = Core::Instance().configCache();
        cache.clearDevices(); // Clear devices cache before fetching to avoid stale configs

        API::ApiRequest request;

        nlohmann::json dbQuery = {
            {jp::TABLE, "devices"},
            {
                jp::COLUMNS, nlohmann::json::array(
                    {"id", "logic_id", "module_id", "name", "type", "config"})
            },
        };

        const API::InternalApi::Target target(API::InternalApi::TargetTypes::DATABASE);
        const API::InternalApi::Method method(API::InternalApi::MethodTypes::GET);
        request.method = API::getTargetMethodString(target.to_string(), method.to_string());

        request.params = dbQuery;
        request.id = Actions::getNextId();


        auto response = co_await sendRequestToDbService(std::move(request));

        if (response.error.has_value()) {
            Core::Instance().mpLogger->errorf(
                "[DATABASE_ACTIONS] [FETCH_DEVICES_CONFIGS] Failed to fetch devices configs: %s",
                response.error.value().data.c_str());
            co_return;
        }

        if (response.result.has_value()) {
            auto responseResultJson = nlohmann::json::parse(response.result.value());

            if (responseResultJson.contains(jp::AFFECTED_ROWS) &&
                responseResultJson.contains(jp::ROWS)) {
                for (const auto &row: responseResultJson[jp::ROWS]) {
                    try {
                        CachedDevice device{
                            .id = row["id"],
                            .logicId = row["logic_id"],
                            .moduleId = row["module_id"],
                            .name = row["name"],
                            .type = row["type"],
                            .config = row["config"]
                        };
                        cache.setDevice(device);
                    } catch (const std::exception &e) {
                        Core::Instance().mpLogger->errorf(
                            "[DATABASE_ACTIONS] [FETCH_DEVICES_CONFIGS] Failed to parse db response: %s", e.what());
                    }
                }
            } else {
                Core::Instance().mpLogger->error(
                    "[DATABASE_ACTIONS] [FETCH_DEVICES_CONFIGS] Invalid db response format");
            }
        }
    }

    ba::awaitable<void> DatabaseActions::fetchAllConfigs() {
        // Make sure cache is cleared before fetching to avoid stale configs
        Core::Instance().readingsCache().clear();
        Core::Instance().configCache().clear();

        co_await fetchModulesConfigs();
        co_await fetchDevicesConfigs();
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

    ba::awaitable<API::ApiResponse> DatabaseActions::sendRequestToDbService(API::ApiRequest &&request) {
        API::InternalApi::Command command(request);
        const auto pCmdMeta = std::make_shared<ActionHelpers::CommandMetadata>(
            command,
            std::make_shared<ba::steady_timer>(Core::Instance().coreUtilityIoContext()),
            Actions::getNextId());

        co_return co_await sendRequestToDbService(std::move(request), pCmdMeta);
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
