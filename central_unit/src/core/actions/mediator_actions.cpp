#include "mediator_actions.h"

namespace SmartHome {
    using ai = API::InternalApi;

    ba::awaitable<API::ApiResponse> MediatorActions::mediatorGetHandler(const cmdMetaPtr &commandMetadata) {
        Core::Instance().mpLogger->debug("[MEDIATOR_ACTIONS] [GET] called");
        const auto &command = commandMetadata->command;

        API::ApiResponse commandResult;
        API::ApiError error;

        commandResult.id = command.commandId;
        error.code = API::ErrorCodes::INVALID_PARAMS;
        error.message = API::errorCodeToString(error.code);

        constexpr uint minNumOfParams = 2;

        if (!areParamsValid(error, command, minNumOfParams)) {
            error.data += "Mediator get command expects params of following format: "
                    "{{'module_id': <uint>}, target_value_key<string>, (optional)optional_arguments<any>}";
            commandResult.error = error;
            co_return commandResult;
        }

        const auto &params = command.params.value();

        API::ApiRequest requestToMediator;
        auto &rtmParams = prepareRequestToMediator(requestToMediator, command);



        if (params[0].is_object() && params[0].contains("module_id")) {
            // TODO fetch module info from storage - temporary hardcoded values for testing
            rtmParams[JsonRpcStrings::ParamsKeys::MODULE_INFO] = nlohmann::json::object({
                {"logic_address", 2},
                {"rf_channel", 2}
            });
            rtmParams[JsonRpcStrings::ParamsKeys::METHOD_PARAMS] = nlohmann::json::array_t(
                params.begin() + 1, params.end());
        } else {
            error.data = "Mediator get command requires first parameter with object containing module id."
                    "Expected format: {{'module_id': <uint>}, target_value_key<string>, (optional)optional_arguments<any>}";
        }

        // TODO Check short term memory for cached result
        commandResult = co_await sendRequestToMediator(std::move(requestToMediator), commandMetadata);
        // TODO Save to short/long term memory

        co_return commandResult;
    }

    ba::awaitable<API::ApiResponse> MediatorActions::mediatorSetHandler(const cmdMetaPtr &commandMetadata) {
        Core::Instance().mpLogger->debug("[MEDIATOR_ACTIONS] [SET] called");
        const auto &command = commandMetadata->command;

        API::ApiResponse commandResult;
        API::ApiError error;

        commandResult.id = command.commandId;
        error.code = API::ErrorCodes::INVALID_PARAMS;
        error.message = API::errorCodeToString(error.code);

        constexpr uint minNumOfParams = 3;

        if (!areParamsValid(error, command, minNumOfParams)) {
            error.data += "Mediator set command expects params of following format: "
                    "{{'module_id': <uint>}, target_value_key<string>, target_new_value<any>}";
            commandResult.error = error;
            co_return commandResult;
        }

        const auto &params = command.params.value();

        API::ApiRequest requestToMediator;
        auto &rtmParams = prepareRequestToMediator(requestToMediator, command);

        if (params[0].is_object() && params[0].contains("module_id")) {
            // TODO fetch module info from storage - temporary hardcoded values for testing
            rtmParams[JsonRpcStrings::ParamsKeys::MODULE_INFO] = nlohmann::json::object({
                {"logic_address", 2},
                {"rf_channel", 2}
            });
            rtmParams[JsonRpcStrings::ParamsKeys::METHOD_PARAMS] = nlohmann::json::array_t(
                params.begin() + 1, params.end());
        } else {
            error.data = "Mediator set command requires first parameter with object containing module id."
                    "Expected format: {{'module_id': <uint>}, target_value_key<string>, target_new_value<any>}";
        }

        // TODO consider if set values should be stored in short/long term memory
        commandResult = co_await sendRequestToMediator(std::move(requestToMediator), commandMetadata);

        co_return commandResult;
    }

    ba::awaitable<API::ApiResponse> MediatorActions::mediatorExecuteHandler(const cmdMetaPtr &commandMetadata) {
        Core::Instance().mpLogger->debug("[MEDIATOR_ACTIONS] [EXECUTE] called");
        const auto &command = commandMetadata->command;

        API::ApiResponse commandResult;
        API::ApiError error;

        commandResult.id = command.commandId;
        error.code = API::ErrorCodes::INVALID_PARAMS;
        error.message = API::errorCodeToString(error.code);

        constexpr uint minNumOfParams = 1;

        if (!areParamsValid(error, command, minNumOfParams)) {
            error.data += "Mediator execute command expects params of following format: "
                    "{(optional){'module_id': <uint>}, action<string>, (optional)action_argument<any>}";
            commandResult.error = error;
            co_return commandResult;
        }

        const auto &params = command.params.value();

        API::ApiRequest requestToMediator;
        auto &rtmParams = prepareRequestToMediator(requestToMediator, command);

        // Check if execute is targeted at module or mediator and handle accordingly
        if (params[0].is_object() && params[0].contains("module_id")) {
            // TODO fetch module info from storage - temporary hardcoded values for testing
            rtmParams[JsonRpcStrings::ParamsKeys::MODULE_INFO] = nlohmann::json::object({
                {"logic_address", 2},
                {"rf_channel", 2}
            });
            rtmParams[JsonRpcStrings::ParamsKeys::METHOD_PARAMS] = nlohmann::json::array_t(
                params.begin() + 1, params.end());
        } else {
            rtmParams[JsonRpcStrings::ParamsKeys::METHOD_PARAMS] =
                    nlohmann::json::array_t(params.begin(), params.end());
        }

        // TODO consider if executed actions should be stored in short/long term memory
        commandResult = co_await sendRequestToMediator(std::move(requestToMediator), commandMetadata);

        co_return commandResult;
    }

    ba::awaitable<API::ApiResponse> MediatorActions::mediatorPingHandler(const cmdMetaPtr &commandMetadata) {
        Core::Instance().mpLogger->debug("[MEDIATOR_ACTIONS] [PING] called");
        const auto &command = commandMetadata->command;

        API::ApiResponse commandResult;
        API::ApiError error;

        commandResult.id = command.commandId;
        error.code = API::ErrorCodes::INVALID_PARAMS;
        error.message = API::errorCodeToString(error.code);

        constexpr uint minNumOfParams = 1;

        if (!areParamsValid(error, command, minNumOfParams)) {
            error.data += "Mediator ping command expects params of following format: "
                    "{{'module_id': <uint>}}";
            commandResult.error = error;
            co_return commandResult;
        }

        const auto &params = command.params.value();

        API::ApiRequest requestToMediator;
        auto &rtmParams = prepareRequestToMediator(requestToMediator, command);

        if (params[0].is_object() && params[0].contains("module_id")) {
            // TODO fetch module info from storage - temporary hardcoded values for testing
            rtmParams[JsonRpcStrings::ParamsKeys::MODULE_INFO] = nlohmann::json::object({
                {"logic_address", 2},
                {"rf_channel", 2}
            });
            rtmParams[JsonRpcStrings::ParamsKeys::METHOD_PARAMS] = nlohmann::json::array_t(
                params.begin() + 1, params.end());
        } else {
            error.data = "Mediator ping command requires parameter with object containing module id."
                    "Expected format: {{'module_id': <uint>}}";
        }

        const auto requestSendTimestamp = std::chrono::system_clock::now();
        commandResult = co_await sendRequestToMediator(std::move(requestToMediator), commandMetadata);
        const auto requestDuration = std::chrono::system_clock::now() - requestSendTimestamp;
        if (!commandResult.error.has_value()) {
            commandResult.result.emplace(
                std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(requestDuration).count()) + "ms");
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
            requestResult.result = response.result;
        } catch (const std::exception &e) {
            requestResult.error = API::ApiError(
                API::ErrorCodes::INTERNAL_ERROR,
                API::errorCodeToString(API::ErrorCodes::INTERNAL_ERROR),
                e.what());
        }

        co_return requestResult;
    }

    bool MediatorActions::areParamsValid(API::ApiError &error, const API::InternalApi::Command &command,
                                         const uint numOfExpectedParams) {
        if (!command.params.has_value()) {
            error.data = "Expected at least " + std::to_string(numOfExpectedParams) + " parameters, none received. ";
            return false;
        }

        const auto &params = command.params.value();
        const auto paramsSize = params.size();
        if (!params.is_array() && paramsSize >= numOfExpectedParams) {
            error.data = "Expected at least " + std::to_string(numOfExpectedParams) + " parameters, " +
                         std::to_string(numOfExpectedParams) + " received. ";
            return false;
        }
        return true;
    }

    nlohmann::json &MediatorActions::prepareRequestToMediator(API::ApiRequest &request,
                                                              const API::InternalApi::Command &command) {
        request.id = command.commandId;
        request.method = command.method.to_string();
        request.params.emplace(nlohmann::json());
        request.params.value()[JsonRpcStrings::ParamsKeys::TARGET] = ai::Target(ai::TargetTypes::MODULE_MEDIATOR).to_string();

        return request.params.value();
    }
}
