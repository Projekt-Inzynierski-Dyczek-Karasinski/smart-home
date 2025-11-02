#include "core_actions.h"
#include "core.h"

#include <memory>
#include <boost/algorithm/string/case_conv.hpp>

using sai = SmartHome::API::InternalApi;
using namespace std::chrono_literals;

namespace SmartHome {
    void CoreActions::handleRequest(const API::InternalApi::Request &request, const RequestCallback &callback) {
        if (!Core::Instance().isRunning()) return;
        const auto logger = Core::Instance().mpLogger;
        const auto requestId = getNextId();

        // TODO add ActiveRequest limit

        // Requests mutex block, attempt to insert new request into msActiveRequest map
        {
            std::scoped_lock lock(msActiveRequestsLock);
            auto [_, inserted] = msActiveRequests.try_emplace(
                requestId,
                request,
                std::make_shared<ba::steady_timer>(
                    Core::Instance().getCoreUtilityIoContext(), ms_REQUEST_TIMEOUT),
                request.commands.size(),
                callback
            );

            if (!inserted) {
                logger->error("[CORE_ACTIONS] Handling new request failed: duplicate request ID");
                return;
            }
        }

        API::ApiError error;

        // Responses mutex block, attempt to prepare response struct in msResponses map
        {
            std::scoped_lock lock(msResponsesLock);
            auto [_, inserted] = msResponses.try_emplace(
                requestId,
                request.connectionId,
                std::vector<API::ApiResponse>{},
                request.isResultStructured
            );

            if (!inserted) {
                logger->error("[CORE_ACTIONS] Handling new request failed: Failed to create response object");
                msActiveRequests.erase(requestId);
                error.code = API::ErrorCodes::INTERNAL_ERROR;
                error.data = "Failed to create response object for request";
            }

            try {
                msResponses[requestId].apiResponses.reserve(request.commands.size());
            } catch (const std::exception &e) {
                logger->errorf("[CORE_ACTIONS] Handling new request failed on reserving responses space: %s", e.what());
                std::scoped_lock lockAR(msActiveRequestsLock);
                msActiveRequests.erase(requestId);
                error.code = API::ErrorCodes::INTERNAL_ERROR;
                error.data = e.what();
            }
        }

        if (error.code != API::ErrorCodes::NO_ERROR) {
            error.message = API::errorCodeToString(error.code);

            API::ApiResponse response;
            response.error = error;
            response.id = API::ApiId(nullptr);

            callback(request.connectionId, response.to_string());
            return;
        }

        msActiveRequests.at(requestId).requestTimeoutTimer.load()->async_wait(
            [requestId](const bs::error_code &ec) {
                // Request mutex block, returns from callback if request is already completed or does not exist
                {
                    std::scoped_lock lock(msActiveRequestsLock);
                    auto iter = msActiveRequests.find(requestId);
                    if (iter == msActiveRequests.end() || iter->second.pendingCommands == 0) return;
                }
                if (!ec) handleRequestTimeout(requestId);
            }
        );

        for (const auto &command: request.commands) {
            const CommandHandler handler = resolveCommand(command);
            executeCommandAsync(handler, command, requestId);
        }
    }

    void CoreActions::onCoreShutdown() {
        auto cleanupTimeout = std::make_shared<ba::steady_timer>(Core::Instance().getCoreUtilityIoContext(),
                                                                 ms_CLEANUP_TIMEOUT);
        std::atomic_bool cleanupTimeoutCalled = false;
        auto cleanup = [&cleanupTimeout, &cleanupTimeoutCalled]() {
            auto expected = false;
            if (cleanupTimeoutCalled.compare_exchange_strong(expected, true)) return;
            cleanupTimeout->cancel();
            std::scoped_lock lock(msActiveRequestsLock, msResponsesLock);
            msActiveRequests.clear();
            msResponses.clear();
        };

        cleanupTimeout->async_wait([cleanup](const bs::error_code &ec) {
            if (!ec) {
                cleanup();
            }
        });

        // Requests mutex block, cancel active request and add command cancelled error result to responses
        {
            std::scoped_lock lock(msActiveRequestsLock);
            for (auto &request: msActiveRequests | std::views::values) {
                if (cleanupTimeoutCalled) break;
                request.cancel();

                for (auto &commandMD: request.commands) {
                    constexpr bool lockMutex = false;
                    if (cleanupTimeoutCalled) break;
                    if (commandMD && commandMD->state == CommandMetadata::State::CANCELLED
                        && commandMD->command.commandId.hasValue()) {
                        API::ApiResponse timeoutResult;
                        timeoutResult.id = commandMD->command.commandId;

                        API::ApiError error;
                        error.code = API::ErrorCodes::INTERNAL_ERROR;
                        error.message = API::errorCodeToString(error.code);
                        error.data = "Command cancelled: Core shutdown cancelled request";

                        timeoutResult.error = error;

                        addCommandResultToResponse(commandMD, std::move(timeoutResult));
                    }
                    updateRequestStatus(commandMD->requestId, lockMutex);
                }
            }
        }
    }

    CoreActions::CommandKey::CommandKey(const sai::TargetTypes newTarget, const sai::MethodTypes newAction) {
        target = newTarget;
        action = newAction;
    }

    CoreActions::CommandKey::CommandKey(const API::InternalApi::Command &command) {
        target = command.target.type;
        action = command.method.type;
    }

    bool CoreActions::CommandKey::operator==(const CommandKey &other) const {
        return target == other.target && action == other.action;
    }


    std::size_t CoreActions::CommandKeyHash::operator()(const CommandKey &key) const {
        return static_cast<size_t>(key.target) << 8 | static_cast<size_t>(key.action);
    }

    CoreActions::CommandMetadata::CommandMetadata(API::InternalApi::Command command,
                                                  std::shared_ptr<ba::steady_timer> commandTimeoutTimer,
                                                  const apiId_t requestId)
        : command(std::move(command)),
          commandTimeoutTimer(std::move(commandTimeoutTimer)),
          requestId(requestId) {
    }

    bool CoreActions::CommandMetadata::cancel() {
        if (auto timer = commandTimeoutTimer.exchange(nullptr)) timer->cancel();
        auto expected = State::PENDING;
        if (state.compare_exchange_strong(expected, State::CANCELLED)) return true;
        return false;
    }


    bool CoreActions::CommandMetadata::isPending() const {
        if (state.load(std::memory_order::relaxed) == State::PENDING) {
            return true;
        }
        return false;
    }


    CoreActions::RequestMetadata::RequestMetadata(API::InternalApi::Request request,
                                                  std::shared_ptr<ba::steady_timer> requestTimeoutTimer,
                                                  const size_t pendingCommands,
                                                  RequestCallback onComplete)
        : request(std::move(request)),
          requestTimeoutTimer(std::move(requestTimeoutTimer)),
          pendingCommands(pendingCommands),
          onComplete(std::move(onComplete)) {
    }

    void CoreActions::RequestMetadata::cancel() {
        if (pendingCommands == 0) return;

        if (auto reqTimer = requestTimeoutTimer.exchange(nullptr)) reqTimer->cancel();

        for (auto &command: commands) {
            command->cancel();
        }
    }

    apiId_t CoreActions::getNextId() {
        static std::atomic<apiId_t> id{0};
        return id.fetch_add(1, std::memory_order::seq_cst) + 1;
    }


    CoreActions::CommandHandler CoreActions::resolveCommand(const API::InternalApi::Command &command) {
        const auto iter = msCommandsRegistry.find(CommandKey(command));
        return iter != msCommandsRegistry.end() ? iter->second : nullptr;
    }

    void CoreActions::executeCommandAsync(CommandHandler handler,
                                          API::InternalApi::Command newCommand,
                                          apiId_t requestId) {
        auto commandMetadata = std::make_shared<CommandMetadata>(
            newCommand,
            std::make_shared<ba::steady_timer>(Core::Instance().getCoreUtilityIoContext()),
            requestId
        );

        // Request mutex block, add command metadata to request metadata
        {
            std::scoped_lock lock(msActiveRequestsLock);
            msActiveRequests.at(requestId).commands.push_back(commandMetadata);
        }


        if (!handler) {
            API::ApiError error;
            auto &&command = commandMetadata->command;

            // Handle parse error signaled by ApiError struct in command.params with UNKNOWN Target and Method
            if (command.params.has_value() &&
                command.target == sai::TargetTypes::UNKNOWN &&
                command.method == sai::MethodTypes::UNKNOWN) {
                try {
                    error = API::ApiError(command.params.value());
                } catch (const std::exception &e) {
                    error.code = API::ErrorCodes::INTERNAL_ERROR;
                    error.data = "Unexpected error while parsing error: " + std::string(e.what());
                }
            }

            if (error.code == API::ErrorCodes::NO_ERROR) {
                if (command.target == sai::TargetTypes::UNKNOWN) {
                    error.code = API::ErrorCodes::INVALID_PARAMS;
                    error.data = "Unknown target - target key value missing or invalid";
                } else if (command.method == sai::MethodTypes::UNKNOWN) {
                    error.code = API::ErrorCodes::METHOD_NOT_FOUND;
                    error.data = "Unknown method - method key value missing or invalid ";
                } else {
                    error.code = API::ErrorCodes::INTERNAL_ERROR;
                    error.data = "Undefined command";
                }
            }

            error.message = errorCodeToString(error.code);

            API::ApiResponse response;
            response.error = error;
            response.id = commandMetadata->command.commandId;

            handleCommandResult(commandMetadata, std::move(response));

            return;
        }

        ba::co_spawn(Core::Instance().getCoreIoContext(),
                     processCommand(commandMetadata, handler),
                     ba::detached);
    }


    ba::awaitable<void> CoreActions::processCommand(std::shared_ptr<CommandMetadata> commandMetadata,
                                                    const CommandHandler handler) {
        API::ApiResponse response;

        try {
            response = co_await handler(commandMetadata);
        } catch (std::exception &exception) {
            API::ApiError error;
            error.code = API::ErrorCodes::INTERNAL_ERROR;
            error.message = API::errorCodeToString(error.code);
            error.data = exception.what();

            response.error = error;
            response.id = commandMetadata->command.commandId;
        }

        if (auto timer = commandMetadata->commandTimeoutTimer.exchange(nullptr)) timer->cancel();
        handleCommandResult(commandMetadata, std::move(response));
        co_return;
    }

    void CoreActions::startCommandTimeoutTimer(const std::shared_ptr<CommandMetadata> &commandMetadata) {
        if (auto timer = commandMetadata->commandTimeoutTimer.load()) {
            timer->expires_after(ms_COMMAND_TIMEOUT);
            timer->async_wait([commandMetadata](const bs::error_code &ec) {
                if (!ec) {
                    handleCommandTimeout(commandMetadata);
                } else {
                    commandMetadata->cancel();
                }
            });
        }
    }


    void CoreActions::handleCommandResult(const std::shared_ptr<CommandMetadata> &commandMetadata,
                                          API::ApiResponse &&commandResult) {
        if (auto timer = commandMetadata->commandTimeoutTimer.exchange(nullptr)) timer->cancel();

        auto expected = CommandMetadata::State::PENDING;
        if (!commandMetadata->state.compare_exchange_strong(expected, CommandMetadata::State::COMPLETED)) {
            return;
        }

        // Add response only if command has an ID
        // As defined in JSON-RPC docs request w/o ID is a notification — response must not be sent in return
        if (!commandMetadata->command.commandId.isUndefined()) {
            addCommandResultToResponse(commandMetadata, std::move(commandResult));
        }
        updateRequestStatus(commandMetadata->requestId);
    }

    void CoreActions::handleCommandTimeout(const std::shared_ptr<CommandMetadata> &commandMetadata) {
        if (auto timer = commandMetadata->commandTimeoutTimer.exchange(nullptr)) timer->cancel();

        auto expected = CommandMetadata::State::PENDING;
        if (!commandMetadata->state.compare_exchange_strong(expected, CommandMetadata::State::TIMED_OUT)) return;

        if (commandMetadata->command.commandId.hasValue()) {
            API::ApiResponse timeoutResponse;
            timeoutResponse.id = commandMetadata->command.commandId;

            API::ApiError error;
            error.code = API::ErrorCodes::INTERNAL_ERROR;
            error.message = API::errorCodeToString(error.code);
            error.data = "Command timeout: exceeded " + std::to_string(ms_COMMAND_TIMEOUT.count()) + "ms timeout";

            timeoutResponse.error = error;

            addCommandResultToResponse(commandMetadata, std::move(timeoutResponse));
        }
        updateRequestStatus(commandMetadata->requestId);


        Core::Instance().mpLogger->errorf("[CORE_ACTIONS] Command timeout - request ID: %d command ID: %s",
                                          commandMetadata->requestId,
                                          commandMetadata->command.commandId.hasValue()
                                              ? std::to_string(commandMetadata->command.commandId.value()).c_str()
                                              : "null");
    }

    void CoreActions::addCommandResultToResponse(const std::shared_ptr<CommandMetadata> &commandMetadata,
                                                 API::ApiResponse &&apiResponse) {
        std::scoped_lock lock(msResponsesLock);
        auto iter = msResponses.find(commandMetadata->requestId);
        if (iter != msResponses.end()) {
            iter->second.apiResponses.push_back(std::move(apiResponse));
        }
    }

    void CoreActions::updateRequestStatus(const apiId_t requestId, const bool lockMutex) {
        if (lockMutex) std::scoped_lock lock(msActiveRequestsLock);
        auto iter = msActiveRequests.find(requestId);
        if (iter != msActiveRequests.end()) {
            auto &request = iter->second;
            if (request.pendingCommands.fetch_sub(1) == 1) {
                if (auto reqTimer = request.requestTimeoutTimer.load()) reqTimer->cancel();
                ba::post(Core::Instance().getCoreIoContext(), [requestId] {
                    handleResponse(requestId);
                    cleanupRequest(requestId);
                });
            }
        }
    }

    void CoreActions::handleRequestTimeout(const apiId_t requestId) {
        Core::Instance().mpLogger->errorf("[CORE_ACTIONS] Request timeout - request ID: %d", requestId);

        std::vector<CommandMetadataPtr> commandsMD;

        // Request mutex block, cancel request and copy commandPtr vector
        {
            std::scoped_lock lock(msActiveRequestsLock);
            auto iter = msActiveRequests.find(requestId);
            if (iter == msActiveRequests.end()) return;

            iter->second.cancel();
            commandsMD = iter->second.commands;
        }

        for (auto &commandMD: commandsMD) {
            if (commandMD && commandMD->state == CommandMetadata::State::CANCELLED
                && commandMD->command.commandId.hasValue()) {
                API::ApiResponse timeoutResult;
                timeoutResult.id = commandMD->command.commandId;

                API::ApiError error;
                error.code = API::ErrorCodes::INTERNAL_ERROR;
                error.message = API::errorCodeToString(error.code);
                error.data = "Command cancelled: request exceeded " + std::to_string(ms_REQUEST_TIMEOUT.count()) +
                             "ms timeout";

                timeoutResult.error = error;

                addCommandResultToResponse(commandMD, std::move(timeoutResult));
                updateRequestStatus(commandMD->requestId);
            }
        }
    }

    void CoreActions::cleanupRequest(const apiId_t requestId) {
        std::scoped_lock lock(msActiveRequestsLock, msResponsesLock);

        auto iter = msActiveRequests.find(requestId);
        if (iter != msActiveRequests.end()) {
            if (auto reqTimer = iter->second.requestTimeoutTimer.load()) reqTimer->cancel();
            for (auto &command: iter->second.commands) {
                if (command == nullptr || command->commandTimeoutTimer.load() == nullptr) continue;
                command->commandTimeoutTimer.load()->cancel();
            }
        }

        msActiveRequests.erase(requestId);
        msResponses.erase(requestId);
    }

    void CoreActions::handleResponse(const apiId_t responseId) {
        const auto logger = Core::Instance().mpLogger;
        RequestCallback requestCallback;
        std::string responseString;
        connectionId_t id;

        std::vector<API::ApiResponse> responsesVector;
        bool isStructured;

        // Responses mutex block, copy responses vector
        {
            std::scoped_lock lock(msResponsesLock);
            const auto iter = msResponses.find(responseId);
            if (iter == msResponses.end()) {
                logger->errorf("[CORE_ACTIONS] [HANDLE_RESPONSE] response for request [ID:%d] not found", responseId);
                return;
            }
            const auto &internalResponse = iter->second;
            responsesVector = internalResponse.apiResponses;
            isStructured = internalResponse.isResultStructured;
        }

        if (responsesVector.empty()) return;

        // Request mutex block, copy onComplete callback and connection id
        {
            std::scoped_lock lock(msActiveRequestsLock);
            const auto iter = msActiveRequests.find(responseId);
            if (iter == msActiveRequests.end()) {
                logger->errorf("[CORE_ACTIONS] [HANDLE_RESPONSE] request [ID:%d] metadata not found", responseId);
                return;
            }
            const auto &requestMetadata = iter->second;
            id = requestMetadata.request.connectionId;
            requestCallback = requestMetadata.onComplete;
        }

        if (isStructured) {
            nlohmann::json json;
            if (responsesVector.size() > 1) {
                json = nlohmann::json::array();
                for (auto &response: responsesVector) {
                    json.push_back(response.to_json());
                }
            } else {
                json = responsesVector[0].to_json();
            }
            responseString = json.dump();
        } else {
            std::string appendToResponse;
            for (auto &response: responsesVector) {
                if (response.result.has_value()) {
                    appendToResponse = response.result.value();
                } else if (response.error.has_value()) {
                    auto &error = response.error.value();
                    appendToResponse = error.message + ": " + error.data;
                } else {
                    appendToResponse = "Internal error: invalid response (missing response or error data)";
                }
                responseString += appendToResponse + ",";
            }
            responseString.erase(responseString.size() - 1);
        }

        requestCallback(id, std::move(responseString));
    }

    std::unordered_map<CoreActions::CommandKey, CoreActions::CommandHandler, CoreActions::CommandKeyHash>
    CoreActions::msCommandsRegistry =
    {
        // TODO implement actions needed for basic functionality
        //Core
        {{sai::TargetTypes::CORE, sai::MethodTypes::GET}, coreGetHandler},
        {{sai::TargetTypes::CORE, sai::MethodTypes::SET}, placeholderHandler},
        {{sai::TargetTypes::CORE, sai::MethodTypes::EXECUTE}, placeholderHandler},
        {{sai::TargetTypes::CORE, sai::MethodTypes::ECHO_REQUEST}, coreEchoHandler},
        {{sai::TargetTypes::CORE, sai::MethodTypes::PING_REQUEST}, placeholderHandler},
        //Mediator
        {{sai::TargetTypes::MODULE_MEDIATOR, sai::MethodTypes::GET}, placeholderHandler},
        {{sai::TargetTypes::MODULE_MEDIATOR, sai::MethodTypes::SET}, placeholderHandler},
        {{sai::TargetTypes::MODULE_MEDIATOR, sai::MethodTypes::EXECUTE}, placeholderHandler},
        {{sai::TargetTypes::MODULE_MEDIATOR, sai::MethodTypes::PING_REQUEST}, placeholderHandler},
        //Database
        {{sai::TargetTypes::DATABASE, sai::MethodTypes::GET}, placeholderHandler},
        {{sai::TargetTypes::DATABASE, sai::MethodTypes::SET}, placeholderHandler},
        {{sai::TargetTypes::DATABASE, sai::MethodTypes::EXECUTE}, placeholderHandler},
        {{sai::TargetTypes::DATABASE, sai::MethodTypes::PING_REQUEST}, placeholderHandler},
        //CLI
        {{sai::TargetTypes::CLI, sai::MethodTypes::SET}, placeholderHandler},
        {{sai::TargetTypes::CLI, sai::MethodTypes::EXECUTE}, placeholderHandler},
        //GUI
        {{sai::TargetTypes::GUI, sai::MethodTypes::SET}, placeholderHandler},
        {{sai::TargetTypes::GUI, sai::MethodTypes::EXECUTE}, placeholderHandler},
        //Webserver
        {{sai::TargetTypes::WEB_SERVER, sai::MethodTypes::GET}, placeholderHandler},
        {{sai::TargetTypes::WEB_SERVER, sai::MethodTypes::SET}, placeholderHandler},
        {{sai::TargetTypes::WEB_SERVER, sai::MethodTypes::EXECUTE}, placeholderHandler},
        {{sai::TargetTypes::WEB_SERVER, sai::MethodTypes::PING_REQUEST}, placeholderHandler}
    };

    std::unordered_map<apiId_t, CoreActions::RequestMetadata> CoreActions::msActiveRequests;

    std::mutex CoreActions::msActiveRequestsLock;

    std::unordered_map<apiId_t, API::InternalApi::Response> CoreActions::msResponses;

    std::mutex CoreActions::msResponsesLock;

    // ======================================== CommandHandler functions ========================================

    // THIS PLACEHOLDER IS AN EXAMPLE AND IT SHOULD BE USED AS A TEMPLATE FOR OTHER COMMAND HANDLERS.
    ba::awaitable<API::ApiResponse> CoreActions::placeholderHandler(
        const std::shared_ptr<CommandMetadata> &commandMetadata) {
        // Optional debug log
        Core::Instance().mpLogger->debug("[CORE_ACTIONS] [PLACEHOLDER] called");
        // Universal command variables' definition.
        const auto &command = commandMetadata->command;
        API::ApiResponse commandResult;
        commandResult.id = command.commandId;

        // Define variables for operation use.
        int operationDuration = 15; // In seconds

        // Check for params and handle them accordingly.
        if (command.params.has_value()) {
            const auto &params = command.params.value();
            if (params.is_array() && !params.empty() && params[0].is_number()) {
                operationDuration = params[0].get<int>();
            }
        }

        // For longer operations use async functions and post any long blocking operations to coreWorkerIoContext.
        // Shorter operations that can be executed without any wait may be implemented as synchronous operations.
        auto promise = std::make_shared<std::promise<std::string> >();
        auto future = promise->get_future();

        // Be sure to implement timeouts and periodic isPending checks to avoid infinitely running operations.
        ba::post(Core::Instance().getCoreWorkerIoContext(), [promise, commandMetadata, operationDuration]() {
            // This example simulates chain of operations with periodic checking for cancellation and command timeout.
            // While command timeout is not needed as request timeout exists, it is advised to use it especially for
            // longer operations in batch requests.
            startCommandTimeoutTimer(commandMetadata);

            for (int i = 0; i < operationDuration * 10; i++) {
                if (!commandMetadata->isPending()) {
                    promise->set_exception(std::make_exception_ptr(std::runtime_error("Operation cancelled")));
                    return;
                }
                std::this_thread::sleep_for(100ms);
            }
            promise->set_value("Operation finished successfully");
        });

        // Wait asynchronously for long operation result.
        while (future.wait_for(25ms) != std::future_status::ready) {
            co_await ba::steady_timer(co_await ba::this_coro::executor, 75ms).async_wait(ba::use_awaitable);
        }

        // Handle and return result or error.
        try {
            commandResult.result = future.get();
        } catch (const std::exception &e) {
            commandResult.error = API::ApiError(
                API::ErrorCodes::INTERNAL_ERROR,
                API::errorCodeToString(API::ErrorCodes::INTERNAL_ERROR).data(),
                e.what()
            );
        }
        co_return commandResult;
    }

    ba::awaitable<API::ApiResponse> CoreActions::coreEchoHandler(
        const std::shared_ptr<CommandMetadata> &commandMetadata) {
        Core::Instance().mpLogger->debug("[CORE_ACTIONS] [CORE_ECHO] called");
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

    ba::awaitable<API::ApiResponse> CoreActions::coreGetHandler(
        const std::shared_ptr<CommandMetadata> &commandMetadata) {
        Core::Instance().mpLogger->debug("[CORE_ACTIONS] [CORE_GET] called");
        const auto &command = commandMetadata->command;
        API::ApiResponse commandResult;
        commandResult.id = command.commandId;


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
                auto indx = conditionParam.find("id");
                if (indx != conditionParam.end()) {
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
}
