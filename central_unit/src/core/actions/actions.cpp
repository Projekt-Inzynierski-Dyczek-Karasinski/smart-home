#include "actions.h"
#include "core_actions.h"
#include "mediator_actions.h"
#include "database_actions.h"

#include <memory>


namespace SmartHome {
    void Actions::handleIncomingRequest(const API::InternalApi::Request &request, const RequestCallback &callback) {
        Core::Instance().mpLogger->debug("[ACTIONS] [HANDLE_INCOMING_REQUEST] called");
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
                    Core::Instance().coreUtilityIoContext(), msREQUEST_TIMEOUT),
                request.commands.size(),
                callback
            );

            if (!inserted) {
                logger->error("[ACTIONS] [HANDLE_INCOMING_REQUEST]  Handling new request failed: duplicate request ID");
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
                logger->error(
                    "[ACTIONS] [HANDLE_INCOMING_REQUEST]  Handling new request failed: Failed to create response object");
                msActiveRequests.erase(requestId);
                error.code = API::ErrorCodes::INTERNAL_ERROR;
                error.data = "Failed to create response object for request";
            }

            try {
                msResponses[requestId].apiResponses.reserve(request.commands.size());
            } catch (const std::exception &e) {
                logger->errorf(
                    "[ACTIONS] [HANDLE_INCOMING_REQUEST] Handling new request failed on reserving responses space: %s",
                    e.what());
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
                    const auto iter = msActiveRequests.find(requestId);
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

    void Actions::handleOutgoingResponse(const apiId_t responseId) {
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
                logger->errorf("[ACTIONS] [HANDLE_OUTGOING_RESPONSE] response for request [ID:%d] not found",
                               responseId);
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
                logger->errorf("[ACTIONS] [HANDLE_OUTGOING_RESPONSE] request [ID:%d] metadata not found", responseId);
                return;
            }
            const auto &requestMetadata = iter->second;
            id = requestMetadata.request.connectionId;
            requestCallback = requestMetadata.onComplete;
        }

        if (isStructured) {
            // Structured response: JSON-RPC object or array.
            nlohmann::json json;
            API::ApiResponse errorResponse;
            errorResponse.id = nullptr;

            API::ApiError error;
            error.code = API::ErrorCodes::INTERNAL_ERROR;
            error.message = API::errorCodeToString(error.code);

            if (responsesVector.size() > 1) {
                // Multiple responses: build JSON array.
                json = nlohmann::json::array();
                for (auto &response: responsesVector) {
                    try {
                        json.push_back(response.to_json());
                    } catch (std::exception &e) {
                        error.data = e.what();
                        errorResponse.error = error;
                        json.push_back(errorResponse.to_json());
                    }
                }
            } else {
                // Single response: serialize directly.
                try {
                    json = responsesVector[0].to_json();
                } catch (std::exception &e) {
                    error.data = e.what();
                    errorResponse.error = error;
                    json = errorResponse.to_json();
                }
            }
            responseString = json.dump();
        } else {
            // Unstructured response: concatenate plain-text results/errors.
            std::string appendToResponse;
            for (auto &response: responsesVector) {
                // Append either result or error message, append Internal error if both are missing
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

    // FIXME segfault can happen when received unexpected response
    void Actions::handleIncomingResponse(const connectionId_t connectionId, const API::ApiResponse &response) {
        Core::Instance().mpLogger->debug("[ACTIONS] [HANDLE_INCOMING_RESPONSE] called");
        if (!response.id.hasValue()) {
            Core::Instance().mpLogger->warningf(
                "[ACTIONS] [HANDLE_INCOMING_RESPONSE] Ignored incoming response for connection [%d] - response id is missing",
                connectionId);
            return;
        }

        std::scoped_lock lock(msOutgoingRequestsLock);

        if (!msOutgoingRequests.contains(connectionId)) {
            Core::Instance().mpLogger->warningf(
                "[ACTIONS] [HANDLE_INCOMING_RESPONSE] Ignored incoming response for connection [%d] - no pending request",
                connectionId);
            return;
        }

        const auto &pendingRequest = msOutgoingRequests.at(connectionId);
        std::scoped_lock lockRequestMetadata(pendingRequest->metadataMutex);

        const auto &responseId = response.id.value();

        if (!pendingRequest->requestsPromises.contains(responseId)) {
            Core::Instance().mpLogger->warningf(
                "[ACTIONS] [HANDLE_INCOMING_RESPONSE] Ignored incoming response for connection [%d] - no pending request with response ID [%d]",
                connectionId,
                responseId);
            return;
        }

        pendingRequest->requestsPromises.at(responseId)->set_value(response);
        pendingRequest->requestsPromises.erase(responseId);

        if (pendingRequest->requestsPromises.empty()) {
            pendingRequest->timeoutTimer->cancel();
        }
    }

    void Actions::handleOutgoingRequest(const connectionId_t connectionId,
                                        API::ApiRequest &&apiRequest,
                                        const std::shared_ptr<std::promise<API::ApiResponse> > &pResponsePromise) {
        Core::Instance().mpLogger->debug("[ACTIONS] [HANDLE_OUTGOING_REQUEST] called");
        std::scoped_lock lockRequestMap(msOutgoingRequestsLock);

        if (!msOutgoingRequests.contains(connectionId)) {
            msOutgoingRequests[connectionId] = std::make_shared<OutgoingRequestMetadata>();
        }
        auto &outgoingRequest = msOutgoingRequests.at(connectionId);
        std::scoped_lock lockRequestMetadata(outgoingRequest->metadataMutex);

        outgoingRequest->sendTimer->cancel();


        outgoingRequest->requestsToSend.push_back(apiRequest);
        if (apiRequest.id.hasValue()) {
            if (!outgoingRequest->requestsPromises.contains(apiRequest.id.value()))
                outgoingRequest->requestsPromises[apiRequest.id.value()] = pResponsePromise;
            else
                Core::Instance().mpLogger->warning(
                    "[ACTIONS] [HANDLE_OUTGOING_REQUEST] ApiRequest with duplicate id ignored");
        }

        const auto timeoutHandler = [outgoingRequest, connectionId](const bs::error_code &timeOutEc) {
            if (!timeOutEc) {
                std::scoped_lock timeoutLock(outgoingRequest->metadataMutex, msOutgoingRequestsLock);
                for (const auto &promise: outgoingRequest->requestsPromises | std::views::values) {
                    promise->set_exception(std::make_exception_ptr(std::runtime_error("Request timeout")));
                }

                if (outgoingRequest->requestsToSend.empty()) {
                    msOutgoingRequests.erase(connectionId);
                }
            }
        };

        const auto sendTimerHandler = [outgoingRequest, connectionId, timeoutHandler](const bs::error_code &sendEc) {
            if (!sendEc) {
                std::scoped_lock sendLock(outgoingRequest->metadataMutex, msOutgoingRequestsLock);
                auto &requests = outgoingRequest->requestsToSend;

                std::string messageToSend;

                if (requests.size() == 1) {
                    messageToSend = requests.front().to_string();
                } else {
                    auto messageJsonArray = nlohmann::json::array();
                    for (const auto &request: requests) {
                        messageJsonArray.push_back(request.to_json());
                    }
                    messageToSend = to_string(messageJsonArray);
                }
                API::InternalApi().handleOutgoing(connectionId, std::move(messageToSend));
                requests.clear();

                outgoingRequest->timeoutTimer->expires_after(msREQUEST_TIMEOUT);
                outgoingRequest->timeoutTimer->async_wait(timeoutHandler);
            }
        };

        // Aggregate outgoing requests
        outgoingRequest->sendTimer->expires_after(msAGGREGATE_OUTGOING_TIMEOUT);
        outgoingRequest->sendTimer->async_wait(sendTimerHandler);
    }

    std::optional<API::InternalApi::Request> Actions::getRequest(const apiId_t requestId) {
        std::scoped_lock lock(msActiveRequestsLock);

        const auto iter = msActiveRequests.find(requestId);

        return iter == msActiveRequests.end() ? std::nullopt : std::optional(iter->second.request);
    }


    apiId_t Actions::getNextId() {
        return API::getNextApiId();
    }

    void Actions::startCommandTimeoutTimer(cmdMetaPtr commandMetadata) {
        // Notifications do not require timeout
        if (commandMetadata->isNotification) {
            return;
        }

        if (const auto timer = commandMetadata->commandTimeoutTimer.load()) {
            timer->expires_after(msCOMMAND_TIMEOUT);
            timer->async_wait([commandMetadata](const bs::error_code &ec) {
                if (!ec) {
                    handleCommandTimeout(commandMetadata);
                } else {
                    commandMetadata->cancel();
                }
            });
        }
    }

    void Actions::onCoreShutdown() {
        auto cleanupTimeout = std::make_shared<ba::steady_timer>(Core::Instance().coreUtilityIoContext(),
                                                                 msCLEANUP_TIMEOUT);
        std::atomic_bool cleanupTimeoutCalled = false;
        auto cleanup = [&cleanupTimeout, &cleanupTimeoutCalled] {
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

        // Requests mutex block, cancel active request and add command canceled error result to responses
        {
            std::scoped_lock lock(msActiveRequestsLock);
            for (auto &request: msActiveRequests | std::views::values) {
                if (cleanupTimeoutCalled) break;
                request.cancel();

                for (auto &commandMD: request.commands) {
                    constexpr bool lockMutex = false;
                    if (cleanupTimeoutCalled) break;
                    if (commandMD && commandMD->state == ActionHelpers::CommandMetadata::State::CANCELLED
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


    Actions::CommandKey::CommandKey(const sai::TargetTypes newTarget, const sai::MethodTypes newAction) {
        target = newTarget;
        action = newAction;
    }


    Actions::CommandKey::CommandKey(const API::InternalApi::Command &command) {
        target = command.target.type;
        action = command.method.type;
    }

    bool Actions::CommandKey::operator==(const CommandKey &other) const {
        return target == other.target && action == other.action;
    }

    std::size_t Actions::CommandKeyHash::operator()(const CommandKey &key) const {
        return static_cast<size_t>(key.target) << 8 | static_cast<size_t>(key.action);
    }


    Actions::RequestMetadata::RequestMetadata(API::InternalApi::Request request,
                                              std::shared_ptr<ba::steady_timer> requestTimeoutTimer,
                                              const size_t pendingCommands,
                                              RequestCallback onComplete)
        : request(std::move(request)),
          requestTimeoutTimer(std::move(requestTimeoutTimer)),
          pendingCommands(pendingCommands),
          onComplete(std::move(onComplete)) {
    }

    void Actions::RequestMetadata::cancel() {
        if (pendingCommands == 0) return;

        if (const auto reqTimer = requestTimeoutTimer.exchange(nullptr)) reqTimer->cancel();

        for (const auto &command: commands) {
            command->cancel();
        }
    }


    Actions::CommandHandler Actions::resolveCommand(const API::InternalApi::Command &command) {
        const auto iter = msCommandsRegistry.find(CommandKey(command));
        return iter != msCommandsRegistry.end() ? iter->second : nullptr;
    }

    void Actions::executeCommandAsync(const CommandHandler &handler,
                                      const API::InternalApi::Command &newCommand,
                                      apiId_t requestId) {
        const auto commandMetadata = std::make_shared<ActionHelpers::CommandMetadata>(
            newCommand,
            std::make_shared<ba::steady_timer>(Core::Instance().coreUtilityIoContext()),
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
                const auto &paramsJson = command.params.value();
                const bool isInApiErrorFormat = paramsJson.is_object() &&
                                                paramsJson.contains(JsonRpcStrings::ErrorKeys::CODE) &&
                                                paramsJson.contains(JsonRpcStrings::ErrorKeys::MESSAGE);
                if (isInApiErrorFormat) {
                    try {
                        error = API::ApiError(paramsJson);
                    } catch (const std::exception &e) {
                        error.code = API::ErrorCodes::INTERNAL_ERROR;
                        error.data = "Unexpected error while parsing error: " + std::string(e.what());
                    }
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

        ba::co_spawn(Core::Instance().coreIoContext(),
                     processCommand(commandMetadata, handler),
                     ba::detached);
    }


    ba::awaitable<void> Actions::processCommand(const cmdMetaPtr commandMetadata,
                                                const CommandHandler handler) {
        if (!commandMetadata->isPending()) co_return; // Skip handling stale commands
        std::optional<API::ApiResponse> response;

        try {
            response = co_await handler(commandMetadata);
        } catch (std::exception &exception) {
            API::ApiError error;
            error.code = API::ErrorCodes::INTERNAL_ERROR;
            error.message = API::errorCodeToString(error.code);
            error.data = exception.what();

            response.emplace(API::ApiResponse());
            response->error = error;
            response->id = commandMetadata->command.commandId;
        }

        if (const auto timer = commandMetadata->commandTimeoutTimer.exchange(nullptr)) timer->cancel();

        if (!commandMetadata->isPending()) co_return;

        if (response.has_value()) handleCommandResult(commandMetadata, std::move(response.value()));

        else updateRequestStatus(commandMetadata->requestId, true);
        co_return;
    }

    void Actions::handleCommandResult(const cmdMetaPtr &commandMetadata,
                                      API::ApiResponse &&commandResult) {
        if (const auto timer = commandMetadata->commandTimeoutTimer.exchange(nullptr)) timer->cancel();

        auto expected = ActionHelpers::CommandMetadata::State::PENDING;
        if (!commandMetadata->state.
            compare_exchange_strong(expected, ActionHelpers::CommandMetadata::State::COMPLETED)) {
            return;
        }

        // As defined in JSON-RPC docs request w/o ID is a notification — response must not be sent in return
        if (!commandMetadata->command.isNotification) {
            addCommandResultToResponse(commandMetadata, std::move(commandResult));
        }
        updateRequestStatus(commandMetadata->requestId);
    }

    void Actions::handleCommandTimeout(const cmdMetaPtr &commandMetadata) {
        if (const auto timer = commandMetadata->commandTimeoutTimer.exchange(nullptr)) timer->cancel();

        auto expected = ActionHelpers::CommandMetadata::State::PENDING;
        if (!commandMetadata->state.compare_exchange_strong(expected, ActionHelpers::CommandMetadata::State::TIMED_OUT))
            return;

        if (commandMetadata->command.commandId.hasValue()) {
            API::ApiResponse timeoutResponse;
            timeoutResponse.id = commandMetadata->command.commandId;

            API::ApiError error;
            error.code = API::ErrorCodes::INTERNAL_ERROR;
            error.message = API::errorCodeToString(error.code);
            error.data = "Command timeout: exceeded " + std::to_string(msCOMMAND_TIMEOUT.count()) + "ms timeout";

            timeoutResponse.error = error;

            addCommandResultToResponse(commandMetadata, std::move(timeoutResponse));
        }
        updateRequestStatus(commandMetadata->requestId);


        Core::Instance().mpLogger->errorf(
            "[ACTIONS] [HANDLE_COMMAND_TIMEOUT] Command timeout - request ID: %d command ID: %s",
            commandMetadata->requestId,
            commandMetadata->command.commandId.hasValue()
                ? std::to_string(commandMetadata->command.commandId.value()).c_str()
                : "null");
    }

    void Actions::addCommandResultToResponse(const cmdMetaPtr &commandMetadata,
                                             API::ApiResponse &&apiResponse) {
        std::scoped_lock lock(msResponsesLock);
        const auto iter = msResponses.find(commandMetadata->requestId);
        if (iter != msResponses.end()) {
            iter->second.apiResponses.push_back(std::move(apiResponse));
        }
    }

    void Actions::updateRequestStatus(const apiId_t requestId, const bool lockMutex) {
        if (lockMutex) std::scoped_lock lock(msActiveRequestsLock);
        const auto iter = msActiveRequests.find(requestId);
        if (iter != msActiveRequests.end()) {
            auto &request = iter->second;
            if (request.pendingCommands.fetch_sub(1) == 1) {
                if (const auto reqTimer = request.requestTimeoutTimer.load()) reqTimer->cancel();
                ba::post(Core::Instance().coreIoContext(), [requestId] {
                    handleOutgoingResponse(requestId);
                    cleanupRequest(requestId);
                });
            }
        }
    }

    void Actions::handleRequestTimeout(const apiId_t requestId) {
        Core::Instance().mpLogger->errorf("[ACTIONS] [HANDLE_REQUEST_TIMEOUT] Request timeout - request ID: %d",
                                          requestId);

        std::vector<cmdMetaPtr> commandsMD;

        // Request mutex block, cancel request and copy commandPtr vector
        {
            std::scoped_lock lock(msActiveRequestsLock);
            const auto iter = msActiveRequests.find(requestId);
            if (iter == msActiveRequests.end()) return;

            iter->second.cancel();
            commandsMD = iter->second.commands;
        }

        for (auto &commandMD: commandsMD) {
            if (commandMD && commandMD->state == ActionHelpers::CommandMetadata::State::CANCELLED
                && commandMD->command.commandId.hasValue()) {
                API::ApiResponse timeoutResult;
                timeoutResult.id = commandMD->command.commandId;

                API::ApiError error;
                error.code = API::ErrorCodes::INTERNAL_ERROR;
                error.message = API::errorCodeToString(error.code);
                error.data = "Command cancelled: request exceeded " + std::to_string(msREQUEST_TIMEOUT.count()) +
                             "ms timeout";

                timeoutResult.error = error;

                addCommandResultToResponse(commandMD, std::move(timeoutResult));
                updateRequestStatus(commandMD->requestId);
            }
        }
    }

    void Actions::cleanupRequest(const apiId_t requestId) {
        std::scoped_lock lock(msActiveRequestsLock, msResponsesLock);

        const auto iter = msActiveRequests.find(requestId);
        if (iter != msActiveRequests.end()) {
            if (const auto reqTimer = iter->second.requestTimeoutTimer.load()) reqTimer->cancel();
            for (auto &command: iter->second.commands) {
                if (command == nullptr || command->commandTimeoutTimer.load() == nullptr) continue;
                command->commandTimeoutTimer.load()->cancel();
            }
        }

        msActiveRequests.erase(requestId);
        msResponses.erase(requestId);
        Core::Instance().mpLogger->debugf("[ACTIONS] [CLEANUP_REQUEST] Request deleted - request ID: %d", requestId);
    }


    std::unordered_map<Actions::CommandKey, Actions::CommandHandler, Actions::CommandKeyHash>
    Actions::msCommandsRegistry =
    {
        // TODO implement actions needed for basic functionality
        // TODO rework handlers/handler invoking to remove redundant code from within handlers
        //Core
        {{sai::TargetTypes::CORE, sai::MethodTypes::GET}, CoreActions::coreGetHandler},
        {{sai::TargetTypes::CORE, sai::MethodTypes::SET}, CoreActions::coreSetHandler},
        {{sai::TargetTypes::CORE, sai::MethodTypes::DELETE}, CoreActions::coreDeleteHandler},
        {{sai::TargetTypes::CORE, sai::MethodTypes::EXECUTE}, placeholderHandler},
        {{sai::TargetTypes::CORE, sai::MethodTypes::NOTIFY}, CoreActions::coreNotifyHandler},
        {{sai::TargetTypes::CORE, sai::MethodTypes::ECHO_REQUEST}, CoreActions::coreEchoHandler},
        {{sai::TargetTypes::CORE, sai::MethodTypes::PING_REQUEST}, placeholderHandler},
        //Mediator
        {{sai::TargetTypes::MODULE_MEDIATOR, sai::MethodTypes::GET}, MediatorActions::mediatorGetHandler},
        {{sai::TargetTypes::MODULE_MEDIATOR, sai::MethodTypes::SET}, MediatorActions::mediatorSetHandler},
        {{sai::TargetTypes::MODULE_MEDIATOR, sai::MethodTypes::EXECUTE}, MediatorActions::mediatorExecuteHandler},
        {{sai::TargetTypes::MODULE_MEDIATOR, sai::MethodTypes::PING_REQUEST}, MediatorActions::mediatorPingHandler},
        //Database
        {{sai::TargetTypes::DATABASE, sai::MethodTypes::GET}, DatabaseActions::databaseRequestHandler},
        {{sai::TargetTypes::DATABASE, sai::MethodTypes::SET}, DatabaseActions::databaseRequestHandler},
        {{sai::TargetTypes::DATABASE, sai::MethodTypes::DELETE}, DatabaseActions::databaseRequestHandler},
        // TODO consider implementing database ping and execute
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

    std::unordered_map<apiId_t, Actions::RequestMetadata> Actions::msActiveRequests;

    std::mutex Actions::msActiveRequestsLock;

    std::unordered_map<connectionId_t, std::shared_ptr<Actions::OutgoingRequestMetadata> > Actions::msOutgoingRequests;

    std::mutex Actions::msOutgoingRequestsLock;

    std::unordered_map<apiId_t, API::InternalApi::Response> Actions::msResponses;

    std::mutex Actions::msResponsesLock;

    std::unordered_map<connectionId_t, sai::TargetTypes> Actions::msConnectionsMap;

    std::shared_mutex Actions::msConnectionsMapLock;

    std::unordered_map<sai::TargetTypes, std::unordered_set<connectionId_t> > Actions::msConnectionTypeMap;

    std::shared_mutex Actions::msConnectionTypeMapLock;

    // ======================================== CommandHandler functions ========================================

    // THIS PLACEHOLDER IS AN EXAMPLE AND IT SHOULD BE USED AS A TEMPLATE FOR OTHER COMMAND HANDLERS.
    awaitOptApiResponse Actions::placeholderHandler(cmdMetaPtr commandMetadata) {
        // Optional debug log
        Core::Instance().mpLogger->debug("[ACTIONS] [PLACEHOLDER_HANDLER] called");
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
        ba::post(Core::Instance().coreWorkerIoContext(), [promise, commandMetadata, operationDuration] {
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
}
