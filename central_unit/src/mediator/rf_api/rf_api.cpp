#include "rf_api.h"
#include "../mediator.h"
#include "rf_client.h"

#include <boost/algorithm/string/case_conv.hpp>
namespace sj = SmartHome::JsonRpcStrings;

namespace SmartHomeMediator {
    using namespace std::string_literals;

    void RfApi::initialize(const std::function<void(const std::string &message)> &messageHandler) {
        mMessageHandler = messageHandler;
    }

    void RfApi::handleIncoming(SmartHome::connectionId_t connectionId, std::string &&message) {
        const auto &pLogger = Mediator::Instance().mpLogger;
        pLogger->debugf("[RF_API] [HANDLE_INCOMING] Incoming message: %s]", message.c_str());

        nlohmann::json jsonRpcRequest;
        RfTypes::SessionMetadata metadata;
        SmartHome::API::ApiResponse response;
        SmartHome::API::ApiError error;
        response.id = nullptr;

        error.code = SmartHome::API::ErrorCodes::PARSE_ERROR;
        error.message = SmartHome::API::errorCodeToString(error.code);

        if (!nlohmann::json::accept(message)) {
            pLogger->error("[RF_API] [HANDLE_INCOMING] Failed to accept incoming message: invalid JSON format");
            return;
        }

        try {
            jsonRpcRequest = nlohmann::json::parse(message);
        } catch (const std::exception &e) {
            pLogger->errorf("[RF_API] [HANDLE_INCOMING] Failed to parse request message from JSON: %s", e.what());
            return;
        }

        // Handle batch request
        if (jsonRpcRequest.is_array()) {
            std::map<uint8_t, RfTypes::SessionMetadata> metadataMap; //{targetLogicAddress: RfTypes::SessionMetadata}

            for (const auto &request: jsonRpcRequest) {
                SmartHome::API::ApiRequest apiRequest;

                try {
                    apiRequest(request);
                    response.id = apiRequest.id;
                } catch (const std::exception &e) {
                    pLogger->errorf("[RF_API] [HANDLE_INCOMING] Failed to parse request message to ApiRequest: %s",
                                    e.what());

                    error.data = "Failed to parse request message to ApiRequest: "s + e.what();
                    response.error = error;

                    handleOutgoing(connectionId, response.to_string());
                    continue;
                }

                try {
                    metadata = toMetadata(apiRequest);
                } catch (const std::exception &e) {
                    error.data = "Failed to parse request into mediator session: "s + e.what();
                    response.error = error;

                    handleOutgoing(connectionId, response.to_string());
                    continue;
                }

                // Join sessions commands if session already exists for target
                if (metadataMap.contains(metadata.targetLogicAddress)) {
                    metadataMap.at(metadata.targetLogicAddress).commands.
                            push_back(std::move(metadata.commands.front()));
                } else {
                    metadataMap[metadata.targetLogicAddress] = std::move(metadata);
                }
            }

            for (auto &m: metadataMap | std::views::values) {
                mpRfClient->addSession(std::move(m));
            }
            return;
        }

        // Handle singular request
        SmartHome::API::ApiRequest apiRequest;

        try {
            apiRequest(jsonRpcRequest);
        } catch (const std::exception &e) {
            pLogger->errorf("[RF_API] [HANDLE_INCOMING] Failed to parse request message to ApiRequest: %s",
                            e.what());
            error.data = "Failed to parse request message to ApiRequest: "s + e.what();
            response.error = error;

            handleOutgoing(connectionId, response.to_string());
            return;
        }

        try {
            metadata = toMetadata(apiRequest);
        } catch (const std::exception &e) {
            error.data = "Failed to parse request into mediator session: "s + e.what();
            response.error = error;

            handleOutgoing(connectionId, response.to_string());
            return;
        }

        mpRfClient->addSession(std::move(metadata));
    }

    void RfApi::handleOutgoing(SmartHome::connectionId_t connectionId, std::string &&message) {
        mMessageHandler(message);
    }

    std::string RfApi::toApiString(RfTypes::RfCommand rfCommand) {
        // Handle response format
        if (rfCommand.rfCommandType == RfTypes::RfCommandType::RESPONSE ||
            rfCommand.rfCommandType == RfTypes::RfCommandType::REPING) {
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
                if (rfCommand.parameters[0].type == RfTypes::ParameterType::ERROR) {
                    // Return MODULE_RUNTIME_ERROR if error occurred on module side
                    error.code = SmartHome::API::ErrorCodes::MODULE_RUNTIME_ERROR;
                    error.message = SmartHome::API::errorCodeToString(error.code);

                    RfTypes::RfErrorCode rfErrorCode;
                    memcpy(&rfErrorCode, rfCommand.parameters[0].value.data(), rfCommand.parameters[0].value.size());
                    error.data = RfTypes::getStringFromRfErrorCode(rfErrorCode);
                    response.error = error;
                } else {
                    // Pass singular response param as result value
                    if (rfCommand.parameters.size() == 1) {
                        response.result = rfCommand.parameters.front().parameterToJson().dump();
                        return response.to_string();
                    }

                    // Store response parameters inside result as an array
                    auto paramJsonArray = nlohmann::json::array();
                    for (const auto &param: rfCommand.parameters) {
                        paramJsonArray.push_back(param.parameterToJson());
                    }
                    response.result = paramJsonArray.dump();
                }
            } else if (rfCommand.rfCommandType == RfTypes::RfCommandType::RESPONSE) {
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
        // Handle notify format
        if (rfCommand.rfCommandType == RfTypes::RfCommandType::NOTIFY) {
            if (!rfCommand.requestType.has_value()) {
                throw std::invalid_argument("RfCommand of notify type does not have NotificationType set");
            }
            SmartHome::API::ApiRequest notify;
            notify.params.emplace();
            auto &params = notify.params.value();

            notify.method = RfTypes::NOTIFY_STRING;

            params[sj::ParamsKeys::TARGET] = RfTypes::CORE_STRING;

            auto notificationType = std::get<RfTypes::NotificationType>(rfCommand.requestType.value());
            auto methodParams = nlohmann::json::array();
            methodParams.push_back(notificationTypeToString(notificationType));
            params[sj::ParamsKeys::METHOD_PARAMS] = methodParams;

            return notify.to_string();
        }

        throw std::invalid_argument("Invalid command");
    }

    std::unique_ptr<RfTypes::Command> RfApi::toRfCommand(const SmartHome::API::ApiRequest &apiRequest,
                                                         const bool isConfigCommand) {
        if (!apiRequest.params.has_value()) {
            throw std::invalid_argument("No parameters specified");
        }
        const auto &params = apiRequest.params.value();
        if (!(params.contains(sj::ParamsKeys::TARGET) && params.at(sj::ParamsKeys::TARGET) ==
              RfTypes::MEDIATOR_STRING)) {
            throw std::invalid_argument("Missing or invalid target parameter");
        }

        std::unique_ptr<nlohmann::json> pMethodParams;
        if (params.contains(sj::ParamsKeys::METHOD_PARAMS)){
            pMethodParams = make_unique<nlohmann::json>(params[sj::ParamsKeys::METHOD_PARAMS]);
        } else if (apiRequest.method != RfTypes::PING_STRING) {
            throw std::invalid_argument("Missing method params");
        }

        auto pRfCommand = std::make_unique<RfTypes::RfCommand>();
        const auto method = boost::algorithm::to_lower_copy(apiRequest.method);

        if (isConfigCommand) return toConfigCommand(apiRequest, method, pMethodParams);

        if (apiRequest.id.hasValue()) {
            parseRequest(pRfCommand.get(), method, pMethodParams.get());
            pRfCommand->requestId = apiRequest.id.value();
            return pRfCommand;
        }

        if (apiRequest.id.isUndefined()) {
            parseNotification(pRfCommand.get(), method, pMethodParams.get());
            return pRfCommand;
        }

        throw std::invalid_argument("Unexpected request format: received request with null ID");
    }

    std::unique_ptr<RfTypes::MediatorConfigCommand> RfApi::toConfigCommand(
        const SmartHome::API::ApiRequest &apiRequest,
        const std::string_view method,
        const std::unique_ptr<nlohmann::json> &pMethodParams) {
        if (!pMethodParams || pMethodParams->empty()) {
            throw std::invalid_argument(
                "Missing method params: method params are required for mediator config command");
        }

        char buffer[1024];
        auto pConfigCommand = std::make_unique<RfTypes::MediatorConfigCommand>();

        if (apiRequest.id.hasValue()) {
            pConfigCommand->requestId = apiRequest.id.value();
        }

        size_t argsRequired = false;

        if (method == RfTypes::GET_STRING) {
            pConfigCommand->configCommandType = RfTypes::MediatorConfigCommandType::GET;
        } else if (method == RfTypes::SET_STRING) {
            pConfigCommand->configCommandType = RfTypes::MediatorConfigCommandType::SET;
            argsRequired = true;
        } else if (method == RfTypes::EXECUTE_STRING) {
            pConfigCommand->configCommandType = RfTypes::MediatorConfigCommandType::EXECUTE;
            argsRequired = true;
        } else {
            throw std::invalid_argument("Invalid method for mediator config command: "s + method.data());
        }

        if (!pMethodParams->contains(sj::MediatorMethodParams::TYPE) || !pMethodParams->at(sj::MediatorMethodParams::TYPE).is_string()) {
            throw std::invalid_argument("Invalid method params: 'type' field is required and must be a string");
        }

        pConfigCommand->commandKey = pMethodParams->at(sj::MediatorMethodParams::TYPE).get<std::string>();

        if (argsRequired) {
            if (!pMethodParams->contains(sj::MediatorMethodParams::ARGS) || !pMethodParams->at(sj::MediatorMethodParams::ARGS).is_array()) {
                throw std::invalid_argument("Invalid method params: 'args' field is required and must be an array");
            }

            pConfigCommand->commandValue = pMethodParams->at(sj::MediatorMethodParams::TYPE).front().is_string()
                                               ? pMethodParams->at(sj::MediatorMethodParams::TYPE).front().get<std::string>()
                                               : to_string(pMethodParams->at(sj::MediatorMethodParams::TYPE).front());
        }

        return pConfigCommand;
    }

    void RfApi::parseRequest(RfTypes::RfCommand *pCommand,
                             const std::string_view method,
                             nlohmann::json *pMethodParams) {
        if (method == RfTypes::PING_STRING) {
            pCommand->rfCommandType = RfTypes::RfCommandType::PING;
            return;
        }

        if (!(pMethodParams &&
              pMethodParams->contains(sj::MediatorMethodParams::TYPE) &&
              pMethodParams->at(sj::MediatorMethodParams::TYPE).is_string())) {
            throw std::invalid_argument("Invalid method specified");
        }

        int paramOffset = 1;

        if (method == RfTypes::SET_STRING) {
            // Handle set command
            pCommand->rfCommandType = RfTypes::RfCommandType::SET;
            // Read set type from 'type' field
            try {
                pCommand->requestType.emplace(
                    RfTypes::setTypeFromString(pMethodParams->at(sj::MediatorMethodParams::TYPE).get<std::string>()));
            } catch (const std::exception &e) {
                throwParseError("set", e.what(), pMethodParams->dump());
            }
        } else if (method == RfTypes::GET_STRING) {
            // Handle get command
            pCommand->rfCommandType = RfTypes::RfCommandType::GET;
            // Read get type from 'type' field
            try {
                pCommand->requestType.emplace(
                    RfTypes::getTypeFromString(pMethodParams->at(sj::MediatorMethodParams::TYPE).get<std::string>()));
            } catch (const std::exception &e) {
                throwParseError("get", e.what(), pMethodParams->dump());
            }
        } else if (method == RfTypes::EXECUTE_STRING) {
            // Handle execute command
            std::string action;
            try {
                action = pMethodParams->at(sj::MediatorMethodParams::TYPE).get<std::string>();
            } catch (const std::exception &e) {
                throwParseError("execute", e.what(), pMethodParams->dump());
            }

            // TODO implement function when adding more actions
            if (action == RfTypes::SLEEP_STRING) {
                pCommand->rfCommandType = RfTypes::RfCommandType::SLEEP;
            } else if (action == RfTypes::DEEP_SLEEP_STRING)
                pCommand->rfCommandType = RfTypes::RfCommandType::DEEP_SLEEP;
            else {
                throw std::invalid_argument("Invalid action parameter specified");
            }
        }


        // Pass args to rfCommand parameter vector
        if (pMethodParams->contains(sj::MediatorMethodParams::ARGS) && pMethodParams->at(sj::MediatorMethodParams::ARGS).is_array()) {
            for (const auto &arg: pMethodParams->at(sj::MediatorMethodParams::ARGS)) {
                pCommand->parameters.push_back(RfTypes::Parameter::parameterFromJson(arg));
            }
        }
    }

    void RfApi::parseNotification(RfTypes::RfCommand *pCommand,
                                  std::string_view method,
                                  nlohmann::json *pMethodParams) {
        if (method != RfTypes::NOTIFY_STRING) {
            throw std::invalid_argument("API notification is not of notify method");
        }

        pCommand->rfCommandType = RfTypes::RfCommandType::NOTIFY;
        // Read notify type from 'type' field
        try {
            pCommand->requestType.emplace(
                RfTypes::notificationTypeFromString(pMethodParams->at(sj::MediatorMethodParams::TYPE).get<std::string>()));
        } catch (const std::exception &e) {
            throwParseError("notify", e.what(), pMethodParams->dump());
        }
    }

    void RfApi::throwParseError(const std::string_view commandType,
                                const std::string_view error,
                                const std::string_view paramsJson) {
        char buffer[1024];
        snprintf(buffer, sizeof(buffer),
                 "Missing or invalid action type inside method_params for %s: %s,\ncurrent method_params: %s",
                 commandType.data(),
                 error.data(),
                 paramsJson.data());
        throw std::invalid_argument(buffer);
    }


    RfTypes::SessionMetadata RfApi::toMetadata(const SmartHome::API::ApiRequest &apiRequest) {
        const auto &pLogger = Mediator::Instance().mpLogger;

        if (!apiRequest.params.has_value()) {
            pLogger->error("[RF_API] [TO_METADATA] Failed to parse incoming message: invalid format");
            throw std::invalid_argument("Invalid format");
        }

        RfTypes::SessionMetadata metadata;
        std::unique_ptr<RfTypes::Command> pCommand;
        const auto &params = apiRequest.params.value();

        // Check for module info object
        if (!params.contains(sj::ParamsKeys::MODULE_INFO)) {
            pLogger->debug("[RF_API] [TO_METADATA] Received request targeted at module mediator");
            metadata.sessionType = RfTypes::SessionType::MEDIATOR_CONFIG;
            metadata.targetLogicAddress = 0;

            // If not module info object is present, try parsing as config command
            try {
                constexpr bool isConfigCommand = true;
                pCommand = toRfCommand(apiRequest, isConfigCommand);
            } catch (const std::exception &e) {
                pLogger->errorf("[RF_API] [TO_METADATA] Failed to parse request message to RfCommand: %s", e.what());
                throw;
            }
            metadata.commands.push_back(std::move(pCommand));

            return metadata;
        }

        const auto &moduleInfo = params.at(sj::ParamsKeys::MODULE_INFO);

        // Check if module info is valid
        if (!(moduleInfo.contains(sj::ModuleInfoKeys::LOGIC_ADDRESS) &&
              moduleInfo.contains(sj::ModuleInfoKeys::RF_CHANNEL))) {
            pLogger->error("[RF_API] [TO_METADATA] Received request with invalid module info");
            throw std::invalid_argument("Invalid module info object");
        }

        // Try parsing command
        try {
            pCommand = toRfCommand(apiRequest);
        } catch (const std::exception &e) {
            pLogger->errorf("[RF_API] [TO_METADATA] Failed to parse request message to RfCommand: %s", e.what());
            throw;
        }

        // Try reading logic address and RF channel from module info and save it to metadata
        try {
            const auto moduleLogicAddress = moduleInfo.at(sj::ModuleInfoKeys::LOGIC_ADDRESS).get<uint8_t>();

            if (moduleLogicAddress == 0) {
                pLogger->error("[RF_API] [TO_METADATA] Module with invalid logic address");
                throw std::invalid_argument("Invalid module info object: module cannot have logic address 0");
            }

            metadata.rfChannel = moduleInfo.at(sj::ModuleInfoKeys::RF_CHANNEL).get<uint8_t>();
            metadata.targetLogicAddress = moduleLogicAddress;
            metadata.commands.push_back(std::move(pCommand));
        } catch (const std::exception &e) {
            pLogger->errorf("[RF_API] [TO_METADATA] Failed to parse request: %s", e.what());
            throw;
        }
        return metadata;
    }
}
