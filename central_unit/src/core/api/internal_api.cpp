#include "internal_api.h"
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

    InternalApi::Method::Method(const std::string_view value) {
        to_action(value);
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

    std::string_view InternalApi::Method::to_string() const {
        switch (type) {
            case MethodTypes::GET:
                return msGET_STRING;
            case MethodTypes::SET:
                return msSET_STRING;
            case MethodTypes::NOTIFY:
                return msNOTIFY_STRING;
            case MethodTypes::EXECUTE:
                return msEXECUTE_STRING;
            case MethodTypes::DELETE:
                return msDELETE_STRING;
            case MethodTypes::ECHO_REQUEST:
                return msECHO_STRING;
            case MethodTypes::PING_REQUEST:
                return msPING_STRING;
            default:
                return msUNKNOWN_STRING;
        }
    }

    void InternalApi::Method::to_action(const std::string_view value) {
        static const std::unordered_map<std::string_view, MethodTypes> strToActMap{
            {msGET_STRING, MethodTypes::GET},
            {msSET_STRING, MethodTypes::SET},
            {msNOTIFY_STRING, MethodTypes::NOTIFY},
            {msEXECUTE_STRING, MethodTypes::EXECUTE},
            {msDELETE_STRING, MethodTypes::DELETE},
            {msECHO_STRING, MethodTypes::ECHO_REQUEST},
            {msPING_STRING, MethodTypes::PING_REQUEST},
            {msUNKNOWN_STRING, MethodTypes::UNKNOWN}
        };

        const auto iter = strToActMap.find(boost::algorithm::to_lower_copy(std::string(value)));
        type = (iter != strToActMap.end()) ? iter->second : MethodTypes::UNKNOWN;
    }


    InternalApi::Target::Target(const TargetTypes value) {
        type = value;
    }

    InternalApi::Target::Target(const std::string_view value) {
        to_target(value);
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

    std::string_view InternalApi::Target::to_string() const {
        switch (type) {
            case TargetTypes::CLI:
                return msCLI_STRING;
            case TargetTypes::GUI:
                return msGUI_STRING;
            case TargetTypes::WEB_SERVER:
                return msWEB_SERVER_STRING;
            case TargetTypes::CORE:
                return msCORE_STRING;
            case TargetTypes::MODULE_MEDIATOR:
                return msMODULE_MEDIATOR_STRING;
            case TargetTypes::DATABASE:
                return msDATABASE_STRING;
            default:
                return msUNKNOWN_STRING;
        }
    }

    void InternalApi::Target::to_target(const std::string_view value) {
        static const std::unordered_map<std::string_view, TargetTypes> strToTargMap{
            {msCLI_STRING, TargetTypes::CLI},
            {msGUI_STRING, TargetTypes::GUI},
            {msWEB_SERVER_STRING, TargetTypes::WEB_SERVER},
            {msWEB_STRING, TargetTypes::WEB_SERVER},
            {msCORE_STRING, TargetTypes::CORE},
            {msMODULE_MEDIATOR_STRING, TargetTypes::MODULE_MEDIATOR},
            {msMEDIATOR_STRING, TargetTypes::MODULE_MEDIATOR},
            {msDATABASE_STRING, TargetTypes::DATABASE}
        };

        const auto iter = strToTargMap.find(boost::algorithm::to_lower_copy(std::string(value)));
        type = (iter != strToTargMap.end()) ? iter->second : TargetTypes::UNKNOWN;
    }


    InternalApi::Command::Command(const ApiRequest &value) {
        if (!value.params.has_value()) {
            throw std::invalid_argument("Command requires params");
        }

        if (!value.params->contains(JsonRpcStrings::ParamsKeys::TARGET)) {
            throw std::invalid_argument("Command params must contain \"target\"");
        }

        target(value.params.value()[JsonRpcStrings::ParamsKeys::TARGET].get<std::string>());

        if (value.params->contains(JsonRpcStrings::ParamsKeys::METHOD_PARAMS))
            params = value.params.value()[JsonRpcStrings::ParamsKeys::METHOD_PARAMS].get<nlohmann::json>();

        commandId = value.id;
        isNotification = !value.id.hasValue();
        method(value.method);
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
                // Handle responses from JSON batch, leave potential requests in jsonMessage
                for (int i = jsonMessage.size() - 1; i >= 0; --i) {
                    try {
                        ApiResponse response(jsonMessage[i]);
                        ba::post(Core::Instance().getCoreIoContext(), [connectionId, response] {
                            Actions::handleIncomingResponse(connectionId, response);
                        });
                        jsonMessage.erase(jsonMessage.begin() + i);
                    } catch (const std::exception &e) {
                        pLogger->debugf("[INTERNAL_API] [HANDLE_INCOMING] Parse to response failed: %s" , e.what());
                    }
                }
                if (jsonMessage.empty()) return;
            } else {
                try {
                    auto response = ApiResponse(jsonMessage);
                    ba::post(Core::Instance().getCoreIoContext(), [connectionId, response] {
                        Actions::handleIncomingResponse(connectionId, response);
                    });
                    return;
                } catch (const std::exception &e) {
                    pLogger->debugf("[INTERNAL_API] [HANDLE_INCOMING] Parse to response failed: %s" , e.what());
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
        ba::post(*IPC::SocketServer::Instance().getIoContext(), [connectionId, message = std::string(message)] {
            auto &socketServer = IPC::SocketServer::Instance();
            if (!socketServer.isRunning()) return;

            const auto connection = socketServer.getConnection(connectionId);
            if (connection != nullptr && connection->isOpen()) {
                connection->writeAsync(message);
            }
        });
    }
}
