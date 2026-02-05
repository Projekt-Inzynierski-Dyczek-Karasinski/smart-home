#include "mediator_actions.h"
#include "database_actions.h"

namespace SmartHome {
    using ai = API::InternalApi;
    namespace jp = JsonRpcStrings::ParamsKeys;
    namespace jmik = JsonRpcStrings::ModuleInfoKeys;
    namespace jr = JsonRpcStrings::ResponseKeys;

    awaitOptApiResponse MediatorActions::mediatorGetHandler(const cmdMetaPtr &commandMetadata) {
        Core::Instance().mpLogger->debug("[MEDIATOR_ACTIONS] [GET] called");
        const auto &command = commandMetadata->command;

        API::ApiResponse commandResult;
        API::ApiError error;

        commandResult.id = command.commandId;
        error.code = API::ErrorCodes::INVALID_PARAMS;
        error.message = API::errorCodeToString(error.code);

        API::ApiRequest requestToMediator;
        auto &rtmParams = prepareRequestToMediator(requestToMediator, command);

        const auto &params = command.params.value();

        auto parsedParams = parseMediatorParams(params, rtmParams);

        if (parsedParams.moduleId.has_value() && !co_await getModuleAddressingInfo(
                rtmParams, parsedParams.moduleId.value(), error.data)) {
            error.code = API::ErrorCodes::INTERNAL_ERROR;
            error.message = API::errorCodeToString(error.code);
            commandResult.error = error;
            co_return commandResult;
        }

        commandResult = co_await sendRequestToMediator(std::move(requestToMediator), commandMetadata);

        // Send to db
        if (parsedParams.moduleId.has_value()) {
            static const std::set<std::string_view> sensorReadApplicableGetTypes = {
                "sensor_value", "force_read_sensor_value", "actuator_value"
            };

            if (commandResult.result.has_value()) {
                DatabaseActions::updateModuleLastOnline(parsedParams.moduleId.value());

                nlohmann::json result;
                try {
                    result = nlohmann::json::parse(commandResult.result.value());
                } catch (const std::exception &e) {
                    Core::Instance().mpLogger->errorf("[MEDIATOR_ACTIONS] [GET] Failed to parse result as JSON: %s",
                                                      e.what());
                    error.code = API::ErrorCodes::INTERNAL_ERROR;
                    error.message = API::errorCodeToString(error.code);
                    error.data = e.what();
                    commandResult.result.reset();
                    commandResult.error = error;
                    co_return commandResult;
                }
                postSensorReadingIfApplicable(parsedParams, result, sensorReadApplicableGetTypes);
            } else {
                postErrorLog(parsedParams.moduleId.value(), commandResult);
            }
        }

        co_return commandResult;
    }

    awaitOptApiResponse MediatorActions::mediatorSetHandler(const cmdMetaPtr &commandMetadata) {
        Core::Instance().mpLogger->debug("[MEDIATOR_ACTIONS] [SET] called");
        const auto &command = commandMetadata->command;

        API::ApiResponse commandResult;
        API::ApiError error;

        commandResult.id = command.commandId;
        error.code = API::ErrorCodes::INVALID_PARAMS;
        error.message = API::errorCodeToString(error.code);

        API::ApiRequest requestToMediator;
        auto &rtmParams = prepareRequestToMediator(requestToMediator, command);

        const auto &params = command.params.value();

        auto parsedParams = parseMediatorParams(params, rtmParams);

        if (parsedParams.moduleId.has_value() && !co_await getModuleAddressingInfo(
                rtmParams, parsedParams.moduleId.value(), error.data)) {
            error.code = API::ErrorCodes::INTERNAL_ERROR;
            error.message = API::errorCodeToString(error.code);
            commandResult.error = error;
            co_return commandResult;
        }

        commandResult = co_await sendRequestToMediator(std::move(requestToMediator), commandMetadata);

        // Send to db
        if (parsedParams.moduleId.has_value()) {
            static const std::set<std::string_view> sensorReadApplicableSetTypes = {
                "toggle_actuator", "set_actuator_value",
            };

            if (commandResult.result.has_value()) {
                DatabaseActions::updateModuleLastOnline(parsedParams.moduleId.value());

                nlohmann::json result;
                try {
                    result = nlohmann::json::parse(commandResult.result.value());
                } catch (const std::exception &e) {
                    Core::Instance().mpLogger->errorf("[MEDIATOR_ACTIONS] [SET] Failed to parse result as JSON: %s",
                                                      e.what());
                    error.code = API::ErrorCodes::INTERNAL_ERROR;
                    error.message = API::errorCodeToString(error.code);
                    error.data = e.what();
                    commandResult.result.reset();
                    commandResult.error = error;
                    co_return commandResult;
                }
                postSensorReadingIfApplicable(parsedParams, result, sensorReadApplicableSetTypes);
            } else {
                postErrorLog(parsedParams.moduleId.value(), commandResult);
            }
        }

        co_return commandResult;
    }


    awaitOptApiResponse MediatorActions::mediatorExecuteHandler(const cmdMetaPtr &commandMetadata) {
        Core::Instance().mpLogger->debug("[MEDIATOR_ACTIONS] [EXECUTE] called");
        const auto &command = commandMetadata->command;

        API::ApiResponse commandResult;
        API::ApiError error;

        commandResult.id = command.commandId;
        error.code = API::ErrorCodes::INVALID_PARAMS;
        error.message = API::errorCodeToString(error.code);

        API::ApiRequest requestToMediator;
        auto &rtmParams = prepareRequestToMediator(requestToMediator, command);

        const auto &params = command.params.value();

        auto parsedParams = parseMediatorParams(params, rtmParams);

        if (parsedParams.moduleId.has_value() && !co_await getModuleAddressingInfo(
                rtmParams, parsedParams.moduleId.value(), error.data)) {
            error.code = API::ErrorCodes::INTERNAL_ERROR;
            error.message = API::errorCodeToString(error.code);
            commandResult.error = error;
            co_return commandResult;
        }

        commandResult = co_await sendRequestToMediator(std::move(requestToMediator), commandMetadata);

        // Send to db
        if (parsedParams.moduleId.has_value()) {
            if (commandResult.result.has_value()) {
                DatabaseActions::updateModuleLastOnline(parsedParams.moduleId.value());

                char buffer[1024];

                std::string action = "<none>";
                std::string actionArgument = "<none>";
                if (parsedParams.type.has_value()) {
                    action = parsedParams.type.value();
                }
                if (parsedParams.args.has_value()) {
                    actionArgument = parsedParams.args.value().dump();
                }

                snprintf(buffer, sizeof(buffer), "Action '%s' with args '%s' executed, with result: %s",
                         action.c_str(), actionArgument.c_str(), commandResult.result.value().c_str());


                DatabaseActions::postLog(parsedParams.moduleId.value(), "info", buffer);
            } else {
                postErrorLog(parsedParams.moduleId.value(), commandResult);
            }
        }

        co_return commandResult;
    }

    awaitOptApiResponse MediatorActions::mediatorPingHandler(const cmdMetaPtr &commandMetadata) {
        Core::Instance().mpLogger->debug("[MEDIATOR_ACTIONS] [PING] called");
        const auto &command = commandMetadata->command;

        API::ApiResponse commandResult;
        API::ApiError error;

        commandResult.id = command.commandId;
        error.code = API::ErrorCodes::INVALID_PARAMS;
        error.message = API::errorCodeToString(error.code);

        API::ApiRequest requestToMediator;
        auto &rtmParams = prepareRequestToMediator(requestToMediator, command);

        const auto &params = command.params.value();

        std::optional<uint> moduleId;
        if (params.contains(jp::MODULE_ID) && params.at(jp::MODULE_ID).is_number())
            moduleId = params.at(jp::MODULE_ID);

        if (moduleId.has_value() && !co_await getModuleAddressingInfo(rtmParams, moduleId.value(), error.data)) {
            error.code = API::ErrorCodes::INTERNAL_ERROR;
            error.message = API::errorCodeToString(error.code);
            commandResult.error = error;
            co_return commandResult;
        }

        const auto requestSendTimestamp = std::chrono::system_clock::now();

        commandResult = co_await sendRequestToMediator(std::move(requestToMediator), commandMetadata);

        const auto requestDuration = std::chrono::system_clock::now() - requestSendTimestamp;

        // Return ping time in ms on received result
        if (!commandResult.error.has_value()) {
            commandResult.result.emplace(
                std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(requestDuration).count()) + "ms");
        }

        if (moduleId.has_value()) {
            if (commandResult.result.has_value()) {
                DatabaseActions::updateModuleLastOnline(moduleId.value());
            } else {
                DatabaseActions::postLog(moduleId.value(), "error", "module ping timeout");
            }
        }

        co_return commandResult;
    }

    ba::awaitable<API::ApiResponse> MediatorActions::sendRequestToMediator(API::ApiRequest &&request,
                                                                           const cmdMetaPtr commandMetadata) {
        auto promise = std::make_shared<std::promise<API::ApiResponse> >();
        auto future = promise->get_future();

        API::ApiResponse requestResult;
        requestResult.id = commandMetadata->command.commandId;

        connectionId_t mediatorConnectionId;
        bool foundMediatorConnectionId = false;

        // Find mediator connection
        {
            // TODO consider handling scenario with multiple mediator connections, or limit them
            std::scoped_lock lock(Actions::msConnectionTypeMapLock);
            auto iter = Actions::msConnectionTypeMap.find(sai::TargetTypes::MODULE_MEDIATOR);
            if (iter != Actions::msConnectionTypeMap.end() && !iter->second.empty()) {
                mediatorConnectionId = *iter->second.begin();
                foundMediatorConnectionId = true;
            }
        }

        if (!foundMediatorConnectionId) {
            requestResult.error = API::ApiError(
                API::ErrorCodes::INTERNAL_ERROR,
                API::errorCodeToString(API::ErrorCodes::INTERNAL_ERROR),
                "Could not find mediator connection");
            co_return requestResult;
        }

        ba::post(Core::Instance().getCoreWorkerIoContext(),
                 [promise, request, commandMetadata, mediatorConnectionId]()mutable {
                     Actions::startCommandTimeoutTimer(commandMetadata);

                     Actions::handleOutgoingRequest(mediatorConnectionId, std::move(request), promise);
                 });

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
        } catch (const std::exception &e) {
            requestResult.error = API::ApiError(
                API::ErrorCodes::INTERNAL_ERROR,
                API::errorCodeToString(API::ErrorCodes::INTERNAL_ERROR),
                e.what());
        }

        co_return requestResult;
    }

    MediatorActions::MediatorRequestParams MediatorActions::parseMediatorParams(const nlohmann::json &incomingParams,
        nlohmann::json &rtmParams) {
        MediatorRequestParams requestParams;

        if (incomingParams.contains(jp::MODULE_ID) && incomingParams.at(jp::MODULE_ID).is_number()) {
            requestParams.moduleId = incomingParams.at(jp::MODULE_ID);
        }

        if (incomingParams.contains(jp::TYPE) && incomingParams.at(jp::TYPE).is_string()) {
            requestParams.type = incomingParams.at(jp::TYPE);
            rtmParams[jp::TYPE] = incomingParams.at(jp::TYPE);
        }

        if (incomingParams.contains(jp::ARGS) && incomingParams.at(jp::ARGS).is_array() && !incomingParams.
            at(jp::ARGS).
            empty()) {
            requestParams.args = incomingParams.at(jp::ARGS);
            rtmParams[jp::ARGS] = incomingParams.at(jp::ARGS);
        }

        return requestParams;
    }

    nlohmann::json &MediatorActions::prepareRequestToMediator(API::ApiRequest &request,
                                                              const API::InternalApi::Command &command) {
        request.id = command.commandId;
        request.method = API::getTargetMethodString(ai::Target(ai::TargetTypes::MODULE_MEDIATOR).to_string(),
                                                    command.method.to_string());

        request.params.emplace(nlohmann::json::object());

        return request.params.value();
    }

    ba::awaitable<bool> MediatorActions::getModuleAddressingInfo(nlohmann::json &preparedParams,
                                                                 uint moduleId,
                                                                 std::string &error) {
        const auto moduleInfo = co_await DatabaseActions::getModuleAddressingInfo(moduleId);

        if (!moduleInfo.contains(jmik::LOGIC_ADDRESS) || !moduleInfo.contains(jmik::RF_CHANNEL)) {
            if (moduleInfo.contains(jr::ERROR)) error = moduleInfo[jr::ERROR];
            else error = "Failed to fetch module info from database: unknown error";
            co_return false;
        }

        preparedParams[jp::MODULE_INFO] = nlohmann::json::object({
            {jmik::LOGIC_ADDRESS, moduleInfo.at(jmik::LOGIC_ADDRESS)},
            {jmik::RF_CHANNEL, moduleInfo.at(jmik::RF_CHANNEL)},
        });

        co_return true;
    }

    void MediatorActions::postSensorReadingIfApplicable(const MediatorRequestParams &parsedParams,
                                                        const nlohmann::json &result,
                                                        const std::set<std::string_view> &applicableTypes) {
        if (!parsedParams.moduleId.has_value()) return;
        if (!parsedParams.type.has_value() || !applicableTypes.contains(parsedParams.type.value())) return;
        if (!parsedParams.args.has_value()) return;
        if (parsedParams.args.value().empty()) return;


        DatabaseActions::postSensorReading(parsedParams.moduleId.value(),
                                           parsedParams.args.value().front(),
                                           result,
                                           {{jp::TYPE, parsedParams.type.value()}});
    }

    void MediatorActions::postErrorLog(const uint moduleId, const API::ApiResponse &result) {
        std::string errStr;
        if (result.error.has_value()) errStr = result.error.value().data;
        else errStr = "Invalid response: no result or error";
        DatabaseActions::postLog(moduleId, "error", errStr);
    }
}
