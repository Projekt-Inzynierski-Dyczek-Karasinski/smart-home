#include "mediator_actions.h"

namespace SmartHome {
    using ai = API::InternalApi;

    ba::awaitable<API::ApiResponse> MediatorActions::mediatorExecuteHandler(
        const std::shared_ptr<Actions::CommandMetadata> &commandMetadata) {
        Core::Instance().mpLogger->debug("[MEDIATOR_ACTIONS] [EXECUTE] called");
        const auto &command = commandMetadata->command;
        API::ApiResponse commandResult;
        API::ApiError error;
        commandResult.id = command.commandId;

        // TODO consider making an function: readParams (throws errors -> catch to error.data)
        error.code = API::ErrorCodes::INVALID_PARAMS;
        error.message = API::errorCodeToString(error.code);
        if (!command.params.has_value()) {
            error.data = "Core set requires params: none received";
            commandResult.error = error;
            co_return commandResult;
        }


        const auto &params = command.params.value();
        const auto paramsSize = params.size();
        if (!(params.is_array() && paramsSize >= 1)) {
            error.data = "Core set requires an array with more than 1 positional parameters "
                    "{(optional){'module_id': <uint>}, action<string>, (optional)action_argument<any>}";
            commandResult.error = error;
            co_return commandResult;
        }

        API::ApiRequest requestToMediator;
        requestToMediator.id = command.commandId;
        requestToMediator.method = command.method.to_string();
        requestToMediator.params.emplace(nlohmann::json());
        auto &rtmParams = requestToMediator.params.value();

        rtmParams[JsonRpcStrings::ParamsKeys::TARGET] = ai::Target(ai::TargetTypes::MODULE_MEDIATOR).to_string();

        if (params[0].is_object() && params[0].contains("module_id")) {
            // TODO fetch module info from storage
            rtmParams[JsonRpcStrings::ParamsKeys::MODULE_INFO] = nlohmann::json::object({
                {"logic_address", params[0]["module_id"]},
                {"rf_channel", 1}
            });
            rtmParams[JsonRpcStrings::ParamsKeys::METHOD_PARAMS] = nlohmann::json::array_t(params.begin()+1, params.end());
        }
        else {
            rtmParams[JsonRpcStrings::ParamsKeys::METHOD_PARAMS] = nlohmann::json::array_t(params.begin(), params.end());

        }

        int mediatorConnectionId = 1; // Read from Actions::msConnectionTypeMap

        auto promise = std::make_shared<std::promise<API::ApiResponse>>();
        auto future = promise->get_future();

        ba::post(Core::Instance().getCoreWorkerIoContext(), [promise, &requestToMediator, commandMetadata, mediatorConnectionId]() {
            Actions::startCommandTimeoutTimer(commandMetadata);

            Actions::handleOutgoinRequest(mediatorConnectionId, std::move(requestToMediator), promise);

        });


        while (commandMetadata->isPending() && future.wait_for(0ms) != std::future_status::ready) {
            co_await ba::steady_timer(co_await ba::this_coro::executor, 10ms).async_wait(ba::use_awaitable);
        }


        // TODO send request and await response, pass response result into commandResult.result


        Core::Instance().mpLogger->debug("[TEST] [MEDIATOR_ACTIONS] [EXECUTE] params" + params.dump());


        commandResult.result = requestToMediator.to_string();
        co_return commandResult;
    }
}
