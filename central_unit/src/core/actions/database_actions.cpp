#include "database_actions.h"

namespace SmartHome {
    using ai = API::InternalApi;

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


        const auto &params = command.params.value();
        API::ApiRequest requestToDatabase;

        auto &rtdParams = prepareRequestToDatabase(requestToDatabase, command);
        rtdParams[JsonRpcStrings::ParamsKeys::METHOD_PARAMS] = params; // Copy original params


        commandResult = co_await sendRequestToDbService(std::move(requestToDatabase), commandMetadata);

        co_return commandResult;
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

        ba::post(Core::Instance().getCoreWorkerIoContext(),
                 [promise, commandMetadata, request, dbServiceConnectionId]()mutable {
                     Actions::startCommandTimeoutTimer(commandMetadata);
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

    nlohmann::json &DatabaseActions::prepareRequestToDatabase(API::ApiRequest &request,
                                                              const API::InternalApi::Command &command) {
        request.id = command.commandId;
        request.method = command.method.to_string();
        request.params.emplace(nlohmann::json());
        request.params.value()[JsonRpcStrings::ParamsKeys::TARGET] = ai::Target(ai::TargetTypes::DATABASE).to_string();

        return request.params.value();
    }
}
