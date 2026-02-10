#include "mediator_actions.h"
#include "database_actions.h"
#include "constants.h"

namespace SmartHome {
    using ai = API::InternalApi;
    namespace jp = JsonRpcStrings::ParamsKeys;
    namespace jmik = JsonRpcStrings::ModuleInfoKeys;
    namespace jr = JsonRpcStrings::ResponseKeys;
    namespace cmck = Constants::ModuleConfigKeys;

    awaitOptApiResponse MediatorActions::mediatorGetHandler(const cmdMetaPtr &commandMetadata) {
        Core::Instance().mpLogger->debug("[MEDIATOR_ACTIONS] [GET] called");
        const auto &command = commandMetadata->command;

        API::ApiResponse commandResult;
        API::ApiError error;

        if (!command.params.has_value() || !command.params->is_object()) {
            commandResult.error = API::ApiError(
                API::ErrorCodes::INVALID_PARAMS,
                errorCodeToString(API::ErrorCodes::INVALID_PARAMS),
                "Mediator get requires params object");
            co_return commandResult;
        }

        const auto &params = command.params.value();

        commandResult.id = command.commandId;
        error.code = API::ErrorCodes::INVALID_PARAMS;
        error.message = API::errorCodeToString(error.code);


        // If module_id is provided, send request to specific module via mediator, otherwise send to mediator directly.
        if (params.contains(jp::MODULE_ID) && params.at(jp::MODULE_ID).is_number_integer()) {
            std::string type;
            if (params.contains(jp::TYPE) && params.at(jp::TYPE).is_string())
                type = params.at(jp::TYPE);

            nlohmann::json args = nlohmann::json::array();
            if (params.contains(jp::ARGS) && params.at(jp::ARGS).is_array())
                args = params.at(jp::ARGS);

            commandResult = co_await sendToModule(
                commandMetadata, params.at(jp::MODULE_ID).get<uint>(), type, args, API::InternalApi::MethodTypes::GET);
            co_return commandResult;
        }

        // If no module_id provided, send get request to mediator itself
        API::ApiRequest request;
        prepareRequestToMediator(request, command);
        commandResult = co_await sendRequestToMediator(std::move(request), commandMetadata);

        co_return commandResult;
    }

    awaitOptApiResponse MediatorActions::mediatorSetHandler(const cmdMetaPtr &commandMetadata) {
        Core::Instance().mpLogger->debug("[MEDIATOR_ACTIONS] [SET] called");
        const auto &command = commandMetadata->command;

        API::ApiResponse commandResult;
        API::ApiError error;

        if (!command.params.has_value() || !command.params->is_object()) {
            commandResult.error = API::ApiError(
                API::ErrorCodes::INVALID_PARAMS,
                errorCodeToString(API::ErrorCodes::INVALID_PARAMS),
                "Mediator set requires params object");
            co_return commandResult;
        }

        const auto &params = command.params.value();

        commandResult.id = command.commandId;
        error.code = API::ErrorCodes::INVALID_PARAMS;
        error.message = API::errorCodeToString(error.code);


        // If module_id is provided, send request to specific module via mediator, otherwise send to mediator directly.
        if (params.contains(jp::MODULE_ID) && params.at(jp::MODULE_ID).is_number_integer()) {
            std::string type;
            if (params.contains(jp::TYPE) && params.at(jp::TYPE).is_string())
                type = params.at(jp::TYPE);

            nlohmann::json args = nlohmann::json::array();
            if (params.contains(jp::ARGS) && params.at(jp::ARGS).is_array())
                args = params.at(jp::ARGS);

            commandResult = co_await sendToModule(
                commandMetadata, params.at(jp::MODULE_ID).get<uint>(), type, args, API::InternalApi::MethodTypes::SET);
            co_return commandResult;
        }

        // If no module_id provided, send set request to mediator itself
        API::ApiRequest request;
        prepareRequestToMediator(request, command);
        commandResult = co_await sendRequestToMediator(std::move(request), commandMetadata);

        co_return commandResult;
    }


    awaitOptApiResponse MediatorActions::mediatorExecuteHandler(const cmdMetaPtr &commandMetadata) {
        Core::Instance().mpLogger->debug("[MEDIATOR_ACTIONS] [EXECUTE] called");
        const auto &command = commandMetadata->command;

        API::ApiResponse commandResult;
        API::ApiError error;

        if (!command.params.has_value() || !command.params->is_object()) {
            commandResult.error = API::ApiError(
                API::ErrorCodes::INVALID_PARAMS,
                errorCodeToString(API::ErrorCodes::INVALID_PARAMS),
                "Mediator execute requires params object");
            co_return commandResult;
        }

        const auto &params = command.params.value();

        commandResult.id = command.commandId;
        error.code = API::ErrorCodes::INVALID_PARAMS;
        error.message = API::errorCodeToString(error.code);

        // If module_id is provided, send request to specific module via mediator, otherwise send to mediator directly.
        if (params.contains(jp::MODULE_ID) && params.at(jp::MODULE_ID).is_number_integer()) {
            auto moduleId = params.at(jp::MODULE_ID).get<uint>();

            std::string type;
            if (params.contains(jp::TYPE) && params.at(jp::TYPE).is_string())
                type = params.at(jp::TYPE);

            nlohmann::json args = nlohmann::json::array();
            if (params.contains(jp::ARGS) && params.at(jp::ARGS).is_array())
                args = params.at(jp::ARGS);

            commandResult = co_await sendToModule(
                commandMetadata, moduleId, type, args, API::InternalApi::MethodTypes::EXECUTE);

            // Log action execution result
            char buffer[1024];
            std::string action = type.empty() ? Constants::Common::NONE_BRACKETS.data() : type;
            std::string actionArgument = args.empty() ? Constants::Common::NONE_BRACKETS.data() : args.dump();

            std::string loggedResult;
            if (commandResult.result.has_value()) {
                loggedResult = "result: " + commandResult.result.value();
            } else if (commandResult.error.has_value()) {
                loggedResult = "error: " + commandResult.error.value().to_string();
            } else {
                loggedResult = "unexpected error: no result or error returned";
            }


            snprintf(buffer, sizeof(buffer), "Action '%s' with args '%s' executed, with %s",
                     action.c_str(), actionArgument.c_str(), loggedResult.c_str());


            DatabaseActions::postLog(moduleId, "info", buffer);
            co_return commandResult;
        }

        // If no module_id provided, execute command on mediator itself
        API::ApiRequest request;
        prepareRequestToMediator(request, command);
        commandResult = co_await sendRequestToMediator(std::move(request), commandMetadata);

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

        // Check for module_id for ping targeting. If not provided or invalid, the ping will be sent to mediator itself.
        std::optional<uint> moduleId;
        if (params.contains(jp::MODULE_ID) && params.at(jp::MODULE_ID).is_number_integer())
            moduleId = params.at(jp::MODULE_ID);

        // Try to get module addressing info if module_id is provided.
        if (moduleId.has_value() && !co_await getModuleAddressingInfo(rtmParams, moduleId.value(), error.data)) {
            error.code = API::ErrorCodes::INTERNAL_ERROR;
            error.message = API::errorCodeToString(error.code);
            commandResult.error = error;
            co_return commandResult;
        }

        // Send request with timestamp for ping time measurement.
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

    ba::awaitable<API::ApiResponse> MediatorActions::sendToModule(const cmdMetaPtr &commandMetadata,
                                                                  uint moduleId,
                                                                  std::string_view type,
                                                                  const nlohmann::json &args,
                                                                  API::InternalApi::MethodTypes method) {
        API::ApiResponse resultResponse;
        API::ApiError error;
        resultResponse.id = commandMetadata->command.commandId;

        API::ApiRequest request;
        auto &rtmParams = prepareRequestToMediator(request, commandMetadata->command);
        rtmParams[jp::TYPE] = type;
        rtmParams[jp::ARGS] = args;


        if (!co_await getModuleAddressingInfo(rtmParams, moduleId, error.data)) {
            error.code = API::ErrorCodes::INTERNAL_ERROR;
            error.message = API::errorCodeToString(error.code);
            resultResponse.error = error;
            co_return resultResponse;
        }

        // Send sleep before request, to allow API to batch it together
        sendSleepIfConfigured(moduleId, rtmParams[jp::MODULE_INFO]);

        resultResponse = co_await sendRequestToMediator(std::move(request), commandMetadata);

        // Send to db
        if (resultResponse.result.has_value()) {
            DatabaseActions::updateModuleLastOnline(moduleId);
            Core::Instance().configCache().updateModuleLastOnline(moduleId, std::chrono::system_clock::now());

            try {
                auto parsed = nlohmann::json::parse(resultResponse.result.value());
                MediatorRequestParams mrp{moduleId, std::string(type), args};
                postSensorReadingIfApplicable(mrp, parsed, getApplicableTypes(method));
            } catch (const std::exception &e) {
                Core::Instance().mpLogger->errorf("[MEDIATOR_ACTIONS] [SET] Failed to parse result as JSON: %s",
                                                  e.what());
                error.code = API::ErrorCodes::INTERNAL_ERROR;
                error.message = API::errorCodeToString(error.code);
                error.data = e.what();
                resultResponse.result.reset();
                resultResponse.error = error;
                co_return resultResponse;
            }
        } else {
            postErrorLog(moduleId, resultResponse);
        }

        co_return resultResponse;
    }


    ba::awaitable<API::ApiResponse> MediatorActions::sendRequestToMediator(API::ApiRequest &&request,
                                                                           const cmdMetaPtr commandMetadata) {
        auto promise = std::make_shared<std::promise<API::ApiResponse> >();
        auto future = promise->get_future();

        API::ApiResponse requestResult;
        requestResult.id = commandMetadata->command.commandId;

        std::optional<connectionId_t> mediatorConnectionId;

        // Find mediator connection
        {
            // TODO consider handling scenario with multiple mediator connections, or limit them
            std::scoped_lock lock(Actions::msConnectionTypeMapLock);
            auto iter = Actions::msConnectionTypeMap.find(sai::TargetTypes::MODULE_MEDIATOR);
            if (iter != Actions::msConnectionTypeMap.end() && !iter->second.empty()) {
                mediatorConnectionId = *iter->second.begin();
            }
        }

        if (!mediatorConnectionId.has_value()) {
            requestResult.error = API::ApiError(
                API::ErrorCodes::INTERNAL_ERROR,
                API::errorCodeToString(API::ErrorCodes::INTERNAL_ERROR),
                "Could not find mediator connection");
            co_return requestResult;
        }

        const auto mediatorId = mediatorConnectionId.value();

        ba::post(Core::Instance().coreWorkerIoContext(),
                 [promise, request, commandMetadata, mediatorId]() mutable {
                     Actions::startCommandTimeoutTimer(commandMetadata);

                     Actions::handleOutgoingRequest(mediatorId, std::move(request), promise);
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

    void MediatorActions::sendSleepIfConfigured(uint moduleId, const nlohmann::json &moduleInfo) {
        const auto configOpt = Core::Instance().configCache().getModule(moduleId);
        if (!configOpt.has_value()) return;

        const auto &config = configOpt->config;
        if (!config.contains(cmck::SLEEP_AFTER_SEND) ||
            !config.at(cmck::SLEEP_AFTER_SEND).is_boolean() ||
            !config.at(cmck::SLEEP_AFTER_SEND))
            return;

        uint sleepDurationMs = 0;
        // Check if there is a scheduled next run time for the module
        const auto nextScheduledRunTimePoint = Core::Instance().scheduler().getNextRunForModule(moduleId);
        if (nextScheduledRunTimePoint.has_value()) {
            sleepDurationMs = static_cast<uint>(
                std::chrono::duration_cast<std::chrono::milliseconds>(nextScheduledRunTimePoint.value() -
                                                                      std::chrono::system_clock::now()).count());
        } else if (config.contains(cmck::DEFAULT_SLEEP_DURATION) &&
                   config.at(cmck::DEFAULT_SLEEP_DURATION).is_number_integer()) {
            // If no scheduled run time, check for default sleep duration in config
            sleepDurationMs = config.at(cmck::DEFAULT_SLEEP_DURATION).get<uint>();
        } else { return; }

        // If no sleep duration configured or duration is 0, do not send sleep
        if (sleepDurationMs == 0) return;

        // Build sleep notification request to mediator
        std::string actionStr = Constants::MediatorTypes::SLEEP.data();
        if (config.contains(cmck::POWER_SAVING) && config.at(cmck::POWER_SAVING).is_boolean() &&
            config.at(cmck::POWER_SAVING)) {
            actionStr = Constants::MediatorTypes::DEEP_SLEEP.data();
        }

        API::ApiRequest sleepNotification;
        sleepNotification.method = API::getTargetMethodString(Constants::Targets::MODULE_MEDIATOR,
                                                              Constants::Methods::EXECUTE);

        API::InternalApi::Command command(sleepNotification);
        auto &params = prepareRequestToMediator(sleepNotification, command);
        params[jp::TYPE] = actionStr;
        params[jp::ARGS] = nlohmann::json::array({sleepDurationMs});
        params[jp::MODULE_INFO] = moduleInfo;

        std::optional<connectionId_t> mediatorConnectionId;

        // Find mediator connection, TODO add function for finding connection
        {
            std::scoped_lock lock(Actions::msConnectionTypeMapLock);
            auto iter = Actions::msConnectionTypeMap.find(sai::TargetTypes::MODULE_MEDIATOR);
            if (iter != Actions::msConnectionTypeMap.end() && !iter->second.empty()) {
                mediatorConnectionId = *iter->second.begin();
            }
        }

        // If no mediator connection found, skip sending sleep notification
        if (!mediatorConnectionId.has_value()) {
            return;
        }

        const auto mediatorId = mediatorConnectionId.value();

        // Post sleep notification to mediator without waiting for response
        ba::post(Core::Instance().coreWorkerIoContext(),
                 [sleepRequest = std::move(sleepNotification), mediatorId]() mutable {
                     Actions::handleOutgoingRequest(mediatorId, std::move(sleepRequest), nullptr);
                 });
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
                                                                 const uint moduleId,
                                                                 std::string &error) {
        nlohmann::json moduleInfo;
        auto module = Core::Instance().configCache().getModule(moduleId);


        if (module.has_value() &&
            module->config.contains(cmck::CONNECTION) &&
            module->config.at(cmck::CONNECTION).contains(jmik::RF_CHANNEL) &&
            module->config.at(cmck::CONNECTION)[jmik::RF_CHANNEL].is_number_integer()) {
            Core::Instance().mpLogger->debugf(
                "[MEDIATOR_ACTIONS] [GET_MODULE_INFO] Fetched module info from cache for module %u",
                moduleId);
            moduleInfo[jmik::LOGIC_ADDRESS] = module->logicAddress;
            moduleInfo[cmck::CONNECTION] =
                    module->config.at(cmck::CONNECTION);
        } else {
            moduleInfo = co_await DatabaseActions::getModuleAddressingInfo(moduleId);
        }


        if (!moduleInfo.contains(jmik::LOGIC_ADDRESS) ||
            !moduleInfo.contains(cmck::CONNECTION) ||
            !moduleInfo[cmck::CONNECTION].contains(jmik::RF_CHANNEL)) {
            if (moduleInfo.contains(jr::ERROR)) {
                error = moduleInfo[jr::ERROR];
            } else {
                error = "Failed to fetch module info from database: unknown error";
            }
            co_return false;
        }

        preparedParams[jp::MODULE_INFO] = nlohmann::json::object({
            {jmik::LOGIC_ADDRESS, moduleInfo.at(jmik::LOGIC_ADDRESS)},
            {jmik::RF_CHANNEL, moduleInfo.at(cmck::CONNECTION).at(jmik::RF_CHANNEL)},
        });

        co_return true;
    }

    void MediatorActions::postSensorReadingIfApplicable(const MediatorRequestParams &parsedParams,
                                                        const nlohmann::json &result,
                                                        const std::set<std::string_view> &applicableTypes) {
        if (!parsedParams.args.has_value() ||
            parsedParams.args.value().empty() ||
            !parsedParams.args.value().front().is_number_integer())
            return;

        const int value = parsedParams.args.value().front().get<uint>();
        uint sensorIdCandidate = value >= 0 ? static_cast<uint>(value) : 0;

        std::optional<uint> sensorIdOpt;
        // Parse first arg as sensor ID when module_id is not provided
        if (!parsedParams.moduleId.has_value()) {
            sensorIdOpt = sensorIdCandidate;
        } else {
            sensorIdOpt = Core::Instance().configCache().findSensorId(parsedParams.moduleId.value(),
                                                                      sensorIdCandidate);
        }

        if (!parsedParams.type.has_value() || !applicableTypes.contains(parsedParams.type.value())) return;


        if (sensorIdOpt.has_value()) {
            Core::Instance().mpLogger->debugf(
                "[MEDIATOR_ACTIONS] [POST_READING] Saving sensor reading to cache and database for sensor ID [%u]",
                sensorIdOpt.value());

            const nlohmann::json readingMetadata = {{jp::TYPE, parsedParams.type.value()}};

            Core::Instance().readingsCache().set(sensorIdOpt.value(), result, readingMetadata);

            DatabaseActions::postSensorReading(sensorIdOpt.value(),
                                               result,
                                               readingMetadata);
            return;
        }
        Core::Instance().mpLogger->warningf(
            "[MEDIATOR_ACTIONS] [POST_READING] Could not find sensor ID for module [%u] "
            "and logic sensor ID [%u] in cache. Skipping saving reading to cache and database.",
            parsedParams.moduleId.value(),
            parsedParams.args.value().front().dump().c_str());
    }

    void MediatorActions::postErrorLog(const uint moduleId, const API::ApiResponse &result) {
        std::string errStr;
        if (result.error.has_value()) errStr = result.error.value().data;
        else errStr = "Invalid response: no result or error";
        DatabaseActions::postLog(moduleId, "error", errStr);
    }

    std::set<std::string_view> MediatorActions::getApplicableTypes(API::InternalApi::MethodTypes method) {
        static const std::set getTypes = {
            Constants::MediatorTypes::SENSOR_VALUE,
            Constants::MediatorTypes::FORCE_READ_SENSOR_VALUE,
            Constants::MediatorTypes::ACTUATOR_VALUE
        };
        static const std::set setTypes = {
            Constants::MediatorTypes::TOGGLE_ACTUATOR,
            Constants::MediatorTypes::SET_ACTUATOR_VALUE
        };
        static const std::set<std::string_view> empty = {};

        switch (method) {
            case API::InternalApi::MethodTypes::GET: return getTypes;
            case API::InternalApi::MethodTypes::SET: return setTypes;
            default: return empty;
        }
    }
}
