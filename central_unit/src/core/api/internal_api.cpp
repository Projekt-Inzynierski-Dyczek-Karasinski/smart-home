#include "internal_api.h"
#include "constants.h"
#include "../core.h"
#include "../actions/actions.h"

#include <unordered_map>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string.hpp>
#include <nlohmann/json_fwd.hpp>

namespace ba = boost::asio;

namespace SmartHome::API {
    InternalApi::Method::Method(const MethodTypes value) {
        type = value;
    }

    InternalApi::Method::Method(const std::string_view value) : type(MethodTypes::UNKNOWN) {
        to_action(value);
    }

    std::string_view InternalApi::Method::to_string() const {
        switch (type) {
            case MethodTypes::GET:
                return Constants::Methods::GET;
            case MethodTypes::SET:
                return Constants::Methods::SET;
            case MethodTypes::NOTIFY:
                return Constants::Methods::NOTIFY;
            case MethodTypes::EXECUTE:
                return Constants::Methods::EXECUTE;
            case MethodTypes::DELETE:
                return Constants::Methods::DELETE;
            case MethodTypes::ECHO_REQUEST:
                return Constants::Methods::ECHO_STR;
            case MethodTypes::PING_REQUEST:
                return Constants::Methods::PING;
            default:
                return Constants::Common::UNKNOWN;
        }
    }

    void InternalApi::Method::to_action(const std::string_view value) {
        static const std::unordered_map<std::string_view, MethodTypes> strToActMap{
            {Constants::Methods::GET, MethodTypes::GET},
            {Constants::Methods::SET, MethodTypes::SET},
            {Constants::Methods::NOTIFY, MethodTypes::NOTIFY},
            {Constants::Methods::EXECUTE, MethodTypes::EXECUTE},
            {Constants::Methods::DELETE, MethodTypes::DELETE},
            {Constants::Methods::ECHO_STR, MethodTypes::ECHO_REQUEST},
            {Constants::Methods::PING, MethodTypes::PING_REQUEST},
            {Constants::Common::UNKNOWN, MethodTypes::UNKNOWN}
        };

        const auto iter = strToActMap.find(boost::algorithm::to_lower_copy(std::string(value)));
        type = iter != strToActMap.end() ? iter->second : MethodTypes::UNKNOWN;
    }

    InternalApi::Method InternalApi::Method::operator()(const std::string_view value) {
        to_action(value);
        return *this;
    }

    bool InternalApi::Method::operator==(const Method &other) const {
        return type == other.type;
    }

    bool InternalApi::Method::operator==(const MethodTypes other) const {
        return type == other;
    }


    InternalApi::Target::Target(const TargetTypes value) {
        type = value;
    }

    InternalApi::Target::Target(const std::string_view value) : type(TargetTypes::UNKNOWN) {
        to_target(value);
    }

    std::string_view InternalApi::Target::to_string() const {
        switch (type) {
            case TargetTypes::CLI:
                return Constants::Targets::CLI;
            case TargetTypes::GUI:
                return Constants::Targets::GUI;
            case TargetTypes::WEB_SERVER:
                return Constants::Targets::WEB_SERVER;
            case TargetTypes::CORE:
                return Constants::Targets::CORE;
            case TargetTypes::MODULE_MEDIATOR:
                return Constants::Targets::MODULE_MEDIATOR;
            case TargetTypes::DATABASE:
                return Constants::Targets::DATABASE;
            default:
                return Constants::Common::UNKNOWN;
        }
    }

    void InternalApi::Target::to_target(const std::string_view value) {
        static const std::unordered_map<std::string_view, TargetTypes> strToTargMap{
            {Constants::Targets::CLI, TargetTypes::CLI},
            {Constants::Targets::GUI, TargetTypes::GUI},
            {Constants::Targets::WEB_SERVER, TargetTypes::WEB_SERVER},
            {Constants::Targets::WEB, TargetTypes::WEB_SERVER},
            {Constants::Targets::CORE, TargetTypes::CORE},
            {Constants::Targets::MODULE_MEDIATOR, TargetTypes::MODULE_MEDIATOR},
            {Constants::Targets::MEDIATOR, TargetTypes::MODULE_MEDIATOR},
            {Constants::Targets::DATABASE, TargetTypes::DATABASE}
        };

        const auto iter = strToTargMap.find(boost::algorithm::to_lower_copy(std::string(value)));
        type = iter != strToTargMap.end() ? iter->second : TargetTypes::UNKNOWN;
    }

    InternalApi::Target InternalApi::Target::operator()(const std::string_view value) {
        to_target(value);
        return *this;
    }

    bool InternalApi::Target::operator==(const Target &other) const {
        return type == other.type;
    }

    bool InternalApi::Target::operator==(const TargetTypes other) const {
        return type == other;
    }


    InternalApi::Command::Command(const ApiRequest &value) {
        // Throws if method string is invalid
        auto [targetStr, methodStr] = parseTargetMethodString(value.method);

        target(targetStr);
        method(methodStr);

        params = value.params.value_or(nlohmann::json::object());
        if (!params->is_object()) {
            throw std::invalid_argument("Invalid JSON-RPC request: params must be an object");
        }

        commandId = value.id;
        isNotification = !value.id.hasValue();
    }

    InternalApi::Command::Command(const nlohmann::json &params,
                                  const ApiId id,
                                  const Method method,
                                  const Target target)
        : params(params), commandId(id), method(method), target(target) {
        isNotification = !id.hasValue();
    }

    void InternalApi::handleIncoming(const apiId_t connectionId, std::string &&message) {
        boost::algorithm::trim(message);
        std::string_view messageView(message);

        const auto pLogger = Core::Instance().mpLogger;
        pLogger->debug("[INTERNAL_API] handleIncoming called");

        bool isMessageJson = false;

        nlohmann::json jsonMessage;

        if (nlohmann::json::accept(messageView)) {
            isMessageJson = true;
            jsonMessage = nlohmann::json::parse(messageView);

            // Check for incoming responses
            if (jsonMessage.is_array()) {
                // Handle responses from JSON batch, leave potential requests in jsonMessage to be processed below
                for (auto i = std::ssize(jsonMessage) - 1; i >= 0; --i) {
                    // Iterate backwards to allow erasing processed responses without affecting unprocessed items
                    const auto &candidate = jsonMessage[i];
                    const bool hasMethod = candidate.contains(JsonRpcStrings::RequestKeys::METHOD);
                    const bool hasResult = candidate.contains(JsonRpcStrings::ResponseKeys::RESULT);
                    const bool hasError = candidate.contains(JsonRpcStrings::ResponseKeys::ERROR);

                    if (!hasMethod && (hasResult || hasError)) {
                        try {
                            ApiResponse response(candidate);
                            ba::post(Core::Instance().coreIoContext(), [connectionId, response] {
                                Actions::handleIncomingResponse(connectionId, response);
                            });
                            jsonMessage.erase(jsonMessage.begin() + i);
                        } catch (const std::exception &e) {
                            pLogger->debugf("[INTERNAL_API] [HANDLE_INCOMING] Parse to response failed: %s", e.what());
                        }
                    }
                }
                if (jsonMessage.empty()) return;
            } else {
                const bool hasMethod = jsonMessage.contains(JsonRpcStrings::RequestKeys::METHOD);
                const bool hasResult = jsonMessage.contains(JsonRpcStrings::ResponseKeys::RESULT);
                const bool hasError = jsonMessage.contains(JsonRpcStrings::ResponseKeys::ERROR);

                if (!hasMethod && (hasResult || hasError)) {
                    try {
                        auto response = ApiResponse(jsonMessage);
                        ba::post(Core::Instance().coreIoContext(), [connectionId, response] {
                            Actions::handleIncomingResponse(connectionId, response);
                        });
                        return;
                    } catch (const std::exception &e) {
                        pLogger->debugf("[INTERNAL_API] [HANDLE_INCOMING] Parse to response failed: %s", e.what());
                    }
                }
            }
        }

        Request requestStruct = {
            .connectionId = connectionId,
        };

        ApiError e;
        if (isMessageJson) {
            //Setting structured result when trimmedRequest contains JSON object
            requestStruct.isResultStructured = true;

            if (jsonMessage.is_array()) {
                // Parse JSON-RPC batch request by filling Request struct commands vector with parsed ApiRequest structs
                for (const auto &unpackedRequest: jsonMessage) {
                    try {
                        requestStruct.commands.emplace_back(ApiRequest(unpackedRequest));
                    } catch (const std::exception &exception) {
                        // On parse error push command with unknown Method and Target with ApiError in params so it is
                        // handled and send back as a part of batch response on request completion
                        requestStruct.commands.emplace_back(
                            ApiError(
                                ErrorCodes::PARSE_ERROR,
                                errorCodeToString(ErrorCodes::PARSE_ERROR).data(),
                                exception.what()
                            ).to_json(),
                            ApiId(nullptr),
                            Method(MethodTypes::UNKNOWN),
                            Target(TargetTypes::UNKNOWN));

                        pLogger->error(
                            "[INTERNAL_API] [HANDLE_INCOMING] JSON-RPC batch request parse error: " +
                            std::string(exception.what()));
                    }
                }
            } else {
                // Parse singular JSON-RPC request
                try {
                    requestStruct.commands.emplace_back(ApiRequest(jsonMessage));
                } catch (const std::exception &exception) {
                    e.code = ErrorCodes::PARSE_ERROR;
                    e.data = exception.what();
                    pLogger->error(
                        "[INTERNAL_API] [HANDLE_INCOMING] JSON-RPC request parse error: " +
                        std::string(exception.what()));
                }
            }
        } else {
            // Parse string-based request (from CLI)
            try {
                requestStruct.commands.emplace_back(ApiRequest(messageView));
            } catch (const std::exception &exception) {
                e.code = ErrorCodes::PARSE_ERROR;
                e.data = exception.what();
                pLogger->error(
                    "[INTERNAL_API] [HANDLE_INCOMING] unstructured request parse error: " +
                    std::string(exception.what()));
            }
        }

        if (e.code != ErrorCodes::NO_ERROR) {
            e.message = errorCodeToString(e.code);
            std::string result;

            if (requestStruct.isResultStructured) {
                ApiResponse errorResponse;
                errorResponse.error = e;
                result = errorResponse.to_string();
            } else {
                result = "[ERROR] " + e.message + " - " + e.data;
            }

            handleOutgoing(connectionId, std::move(result));
            return;
        }

        pLogger->debug("[INTERNAL_API] handler call");
        Actions::handleIncomingRequest(requestStruct, [this](const connectionId_t id, std::string &&response) {
            handleOutgoing(id, std::move(response));
        });
    }

    void InternalApi::handleOutgoing(const apiId_t connectionId, std::string &&message) {
        Core::Instance().mpLogger->debug("[INTERNAL_API] handle outgoing called");
        Core::Instance().mpLogger->debug("[INTERNAL_API] outgoing: " + message);
        ba::post(*IPC::SocketServer::Instance().getIoContext(), [connectionId, message = std::move(message)] mutable {
            auto &socketServer = IPC::SocketServer::Instance();
            if (!socketServer.isRunning()) return;

            const auto connection = socketServer.getConnection(connectionId);
            if (connection != nullptr && connection->isOpen()) {
                try {
                    connection->writeAsync(std::move(message));
                } catch (const std::exception &e) {
                    Core::Instance().mpLogger->errorf("[INTERNAL_API] [HANDLE_OUTGOING] async write failed: %s", e.what());
                }
            }
        });
    }
}
