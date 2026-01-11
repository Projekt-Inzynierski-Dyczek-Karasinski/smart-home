#include "rf_api.h"
#include "mediator.h"
#include "rf_client.h"

#include <boost/algorithm/string/case_conv.hpp>
namespace sj = SmartHome::JsonRpcStrings;

namespace SmartHomeMediator {
    RfTypes::RfCommand RfApi::toRfCommand(const SmartHome::API::ApiRequest &apiRequest) {
        RfTypes::RfCommand rfCommand;
        char buffer[1024];

        if (!apiRequest.params.has_value()) {
            throw std::invalid_argument("No parameters specified");
        }
        const auto &params = apiRequest.params.value();
        if (!(params.contains(sj::ParamsKeys::TARGET) && params.at(sj::ParamsKeys::TARGET) ==
              RfTypes::MEDIATOR_STRING)) {
            throw std::invalid_argument("Missing or invalid target parameter");
        }

        std::unique_ptr<nlohmann::json> methodParams;
        if (params.contains(sj::ParamsKeys::METHOD_PARAMS)) {
            methodParams = make_unique<nlohmann::json>(params[sj::ParamsKeys::METHOD_PARAMS]);
        } else {
            throw std::invalid_argument("Missing method params");
        }

        const auto method = boost::algorithm::to_lower_copy(apiRequest.method);

        if (apiRequest.id.hasValue()) {
            if (method == RfTypes::SET_STRING && methodParams != nullptr && methodParams->is_array() &&
                methodParams->size() > 1) {
                rfCommand.commandType = RfTypes::CommandTypes::SET;
                // Read set type from first index
                try {
                    rfCommand.requestType.emplace(
                        RfTypes::setTypeFromString(methodParams.get()->front().get<std::string>()));
                } catch (const std::exception &e) {
                    sprintf(buffer,
                            "Missing or invalid action type inside method_params for set: %s,\ncurrent method_params: %s",
                            e.what(),
                            methodParams.get()->dump().c_str());
                    throw std::invalid_argument(buffer);
                }
            } else if (method == RfTypes::GET_STRING && methodParams != nullptr && methodParams->is_array() &&
                       !methodParams->empty()) {
                rfCommand.commandType = RfTypes::CommandTypes::GET;
                // Read get type from first index
                try {
                    rfCommand.requestType.emplace(
                        RfTypes::getTypeFromString(methodParams.get()->front().get<std::string>()));
                } catch (const std::exception &e) {
                    sprintf(buffer,
                            "Missing or invalid action type inside method_params for get: %s,\ncurrent method_params: %s",
                            e.what(),
                            methodParams.get()->dump().c_str());
                    throw std::invalid_argument(buffer);
                }
            } else if (method == RfTypes::EXECUTE_STRING && methodParams != nullptr && methodParams->is_array() &&
                       !methodParams->empty()) {
                std::string action;
                try {
                    action = methodParams.get()->front().get<std::string>();
                } catch (const std::exception &e) {
                    sprintf(buffer,
                            "Missing or invalid action type inside method_params for execute: %s,\ncurrent method_params: %s",
                            e.what(),
                            methodParams.get()->dump().c_str());
                    throw std::invalid_argument(buffer);
                }

                if (action == RfTypes::SLEEP_STRING) rfCommand.commandType = RfTypes::CommandTypes::SLEEP;
                else if (action == RfTypes::DEEP_SLEEP_STRING)
                    rfCommand.commandType = RfTypes::CommandTypes::DEEP_SLEEP;
                else {
                    throw std::invalid_argument("Invalid action parameter specified");
                }
            } else if (method == RfTypes::PING_STRING) {
                rfCommand.commandType = RfTypes::CommandTypes::PING;
            } else {
                throw std::invalid_argument("Invalid method specified");
            }

            // Pass remaining method params to rfCommand parameter vector
            for (auto i = 1; i < methodParams->size(); i++) {
                rfCommand.parameters.push_back(RfTypes::Parameter::parameterFromJson(methodParams.get()->at(i)));
            }

            rfCommand.requestId = apiRequest.id.value();
        } else if (apiRequest.id.isUndefined()) {
            if (method == RfTypes::NOTIFY_STRING) {
                rfCommand.commandType = RfTypes::CommandTypes::NOTIFY;
                // Read get notify from first index
                try {
                    rfCommand.requestType.emplace(
                        RfTypes::notificationTypeFromString(methodParams.get()->front().get<std::string>()));
                } catch (const std::exception &e) {
                    sprintf(buffer,
                            "Missing or invalid action type inside method_params for notify: %s,\ncurrent method_params: %s",
                            e.what(),
                            methodParams.get()->dump().c_str());
                    throw std::invalid_argument(buffer);
                }
            } else {
                throw std::invalid_argument("API notification is not of notify method");
            }
        } else {
            throw std::invalid_argument("Unexpected request format: received request with null ID");
        }

        return rfCommand;
    }

    std::string RfApi::toApiString(RfTypes::RfCommand rfCommand) {
        if (rfCommand.commandType == RfTypes::CommandTypes::RESPONSE || rfCommand.commandType ==
            RfTypes::CommandTypes::REPING) {
            SmartHome::API::ApiError error;
            SmartHome::API::ApiResponse response;
            if (rfCommand.requestId.has_value()) {
                response.id = rfCommand.requestId.value();
            } else {
                // Set response id as null when command id is not set (error response)
                response.id = nullptr;
            }

            if (!rfCommand.parameters.empty()) {
                // Check for error
                if (rfCommand.parameters[0].type == RfTypes::ParameterTypes::ERROR) {
                    // Return MODULE_RUNTIME_ERROR if error occurred on module side
                    error.code = SmartHome::API::ErrorCodes::MODULE_RUNTIME_ERROR;
                    error.message = SmartHome::API::errorCodeToString(error.code);

                    RfTypes::RfErrorCodes rfErrorCode;
                    memcpy(&rfErrorCode, rfCommand.parameters[0].value.data(), rfCommand.parameters[0].value.size());
                    error.data = RfTypes::getStringFromRfErrorCode(rfErrorCode);
                    response.error = error;
                } else {
                    // Store response parameters inside result as an array
                    auto paramJsonArray = nlohmann::json::array();
                    for (const auto &param: rfCommand.parameters) {
                        paramJsonArray.push_back(param.parameterToJson());
                    }
                    response.result = paramJsonArray.dump();
                }
            } else if (rfCommand.commandType == RfTypes::CommandTypes::RESPONSE) {
                // Return MEDIATOR_COMMUNICATION_ERROR if response has no parameters
                error.code = SmartHome::API::ErrorCodes::MEDIATOR_COMMUNICATION_ERROR;
                error.message = SmartHome::API::errorCodeToString(error.code);
                error.data = "Module response without values";

                response.error = error;
            } else {
                response.result.emplace(); // REPING without values is a valid response
            }
            // Return response
            return response.to_string();
        }
        if (rfCommand.commandType == RfTypes::CommandTypes::NOTIFY) {
            if (!rfCommand.requestType.has_value()) {
                throw std::invalid_argument("RfCommand of notify type does not have NotificationType set");
            }

            SmartHome::API::ApiRequest notify;
            auto &params = notify.params.value();

            notify.method = "notify";

            params[sj::ParamsKeys::TARGET] = "core";

            auto notificationType = std::get<RfTypes::NotificationTypes>(rfCommand.requestType.value());
            params[sj::ParamsKeys::METHOD_PARAMS] = notificationTypeToString(notificationType);

            return notify.to_string();
        }

        throw std::invalid_argument("Invalid command");
    }

    RfApi::RfApi(const std::shared_ptr<RfClient> &pRfClient) : mpRfClient(pRfClient) {
    }

    void RfApi::initialize(const std::function<void(const std::string &message)> &messageHandler) {
        mMessageHandler = messageHandler;
    }

    void RfApi::handleIncoming(SmartHome::connectionId_t connectionId, std::string &&message) {
        const auto &pLogger = Mediator::Instance().mpLogger;
        pLogger->debugf("[RF_API] [HANDLE_INCOMING] Incoming message: %s]", message.c_str());

        nlohmann::json jsonRpcRequest;

        try {
            jsonRpcRequest = nlohmann::json::parse(message);
        } catch (const std::exception &e) {
            pLogger->errorf("[RF_API] [HANDLE_INCOMING] Failed to parse request message from json: %s", e.what());
            return;
        }

        //TODO add api commands for mediator remote control
        //TODO send error responses

        if (jsonRpcRequest.is_array()) {
            // Handle batch request
            std::map<uint8_t, RfTypes::SessionMetadata> metadataMap; //{targetLogicAddress: RfTypes::SessionMetadata}

            for (const auto &request: jsonRpcRequest) {
                SmartHome::API::ApiRequest apiRequest;

                try {
                    apiRequest(request);
                } catch (const std::exception &e) {
                    pLogger->errorf("[RF_API] [HANDLE_INCOMING] Failed to parse request message to ApiRequest: %s",
                                    e.what());
                    continue;
                }

                auto metadata = toMetadata(apiRequest);
                if (!metadata.has_value()) continue;
                const auto &metadataValue = metadata.value();

                if (metadataMap.contains(metadataValue.targetLogicAddress)) {
                    metadataMap.at(metadataValue.targetLogicAddress).commands.push_back(metadataValue.commands.front());
                } else {
                    metadataMap[metadataValue.targetLogicAddress] = metadataValue;
                }
            }

            for (auto &metadata: metadataMap | std::views::values) {
                mpRfClient->addSession(std::move(metadata));
            }
        } else {
            SmartHome::API::ApiRequest apiRequest;

            try {
                apiRequest(jsonRpcRequest);
            } catch (const std::exception &e) {
                pLogger->errorf("[RF_API] [HANDLE_INCOMING] Failed to parse request message to ApiRequest: %s",
                                e.what());
                return;
            }

            auto metadata = toMetadata(apiRequest);
            if (!metadata.has_value()) return;;

            mpRfClient->addSession(std::move(metadata.value()));
        }
    }

    void RfApi::handleOutgoing(SmartHome::connectionId_t connectionId, std::string &&message) {
        mMessageHandler(message);
    }

    std::optional<RfTypes::SessionMetadata> RfApi::toMetadata(const SmartHome::API::ApiRequest &apiRequest) {
        const auto &pLogger = Mediator::Instance().mpLogger;

        RfTypes::SessionMetadata metadata;
        RfTypes::RfCommand rfCommand;

        if (!apiRequest.params.has_value()) {
            pLogger->error("[RF_API] [TO_METADATA] Failed to parse incoming message: invalid format");
            return std::nullopt;
        }

        const auto &params = apiRequest.params.value();

        if (!params.contains(sj::ParamsKeys::MODULE_INFO)) {
            pLogger->error("[RF_API] [TO_METADATA] Received request without module info");
            return std::nullopt;
        }

        const auto &moduleInfo = params.at(sj::ParamsKeys::MODULE_INFO);

        if (!(moduleInfo.contains(sj::ModuleInfoKeys::LOGIC_ADDRESS) &&
              moduleInfo.contains(sj::ModuleInfoKeys::RF_CHANNEL))) {
            pLogger->error("[RF_API] [TO_METADATA] Received request with invalid module info");
            return std::nullopt;
        }

        try {
            rfCommand = toRfCommand(apiRequest);
        } catch (const std::exception &e) {
            pLogger->errorf("[RF_API] [TO_METADATA] Failed to parse request message to RfCommand: %s", e.what());
            return std::nullopt;
        }

        try {
            metadata.rfChannel = moduleInfo.at(sj::ModuleInfoKeys::RF_CHANNEL).get<uint8_t>();
            metadata.targetLogicAddress = moduleInfo.at(sj::ModuleInfoKeys::LOGIC_ADDRESS).get<uint8_t>();
            metadata.commands.push_back(rfCommand);
        } catch (const std::exception &e) {
            pLogger->errorf("[RF_API] [TO_METADATA] Failed to parse request: %s", e.what());
            return std::nullopt;
        }
        return metadata;
    }
}
