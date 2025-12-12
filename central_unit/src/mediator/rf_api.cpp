#include "rf_api.h"
#include "mediator.h"
#include "rf_client.h"

#include <boost/algorithm/string/case_conv.hpp>

namespace sj = SmartHome::JsonRpcStrings;

namespace SmartHomeMediator {
    RfApi::Parameter::Parameter(const uint64_t newValue) {
        type = ParameterTypes::UINT;

        if (newValue <= std::numeric_limits<uint8_t>::max()) {
            assignSwappedEndian(value, static_cast<uint8_t>(newValue));
        } else if (newValue <= std::numeric_limits<uint16_t>::max()) {
            assignSwappedEndian(value, static_cast<uint16_t>(newValue));
        } else if (newValue <= std::numeric_limits<uint32_t>::max()) {
            assignSwappedEndian(value, static_cast<uint32_t>(newValue));
        } else {
            assignSwappedEndian(value, newValue);
        }
    }

    RfApi::Parameter::Parameter(const int64_t newValue) {
        type = ParameterTypes::INT;

        if (newValue >= std::numeric_limits<int8_t>::min() && newValue <= std::numeric_limits<int8_t>::max()) {
            assignSwappedEndian(value, static_cast<int8_t>(newValue));
        } else if (newValue >= std::numeric_limits<int16_t>::min() && newValue <= std::numeric_limits<int16_t>::max()) {
            assignSwappedEndian(value, static_cast<int16_t>(newValue));
        } else if (newValue >= std::numeric_limits<int32_t>::min() && newValue <= std::numeric_limits<int32_t>::max()) {
            assignSwappedEndian(value, static_cast<int32_t>(newValue));
        } else {
            assignSwappedEndian(value, newValue);
        }
    }

    RfApi::Parameter::Parameter(const double newValue) {
        type = ParameterTypes::FLOAT;

        if (newValue <= std::numeric_limits<float>::max()) {
            assignSwappedEndian(value, static_cast<float>(newValue));
        } else {
            assignSwappedEndian(value, newValue);
        }
    }

    RfApi::Parameter::Parameter(const std::string_view newValue) {
        type = ParameterTypes::ASCII;

        value.assign(newValue.begin(), newValue.end());
    }

    RfApi::Parameter::Parameter(const std::vector<uint8_t> &newValue) {
        type = ParameterTypes::RAW;

        value = newValue;
    }

    RfApi::Parameter::Parameter(const RfErrorCodes newValue) {
        type = ParameterTypes::ERROR;

        assignSwappedEndian(value, static_cast<uint8_t>(newValue));
    }

    RfApi::Parameter RfApi::Parameter::parameterFromJson(const nlohmann::json &json) {
        if (json.is_number_unsigned()) return Parameter(json.get<uint64_t>());
        if (json.is_number_integer()) return Parameter(json.get<int64_t>());
        if (json.is_number_float()) return Parameter(json.get<double>());
        if (json.is_string()) return Parameter(json.get<std::string>());
        if (json.is_array()) return Parameter(json.get<std::vector<uint8_t> >());
        throw std::runtime_error("unsupported JSON parameter type");
    }

    nlohmann::json RfApi::Parameter::parameterToJson() const {
        size_t size = value.size();
        switch (type) {
            case ParameterTypes::UINT:
                if (size == sizeof(uint8_t)) return getValueFromRawData<uint8_t>(value);
                if (size == sizeof(uint16_t)) return getValueFromRawData<uint16_t>(value);
                if (size == sizeof(uint32_t)) return getValueFromRawData<uint32_t>(value);
                return getValueFromRawData<uint64_t>(value);
            case ParameterTypes::INT:
                if (size == sizeof(int8_t)) return getValueFromRawData<int8_t>(value);
                if (size == sizeof(int16_t)) return getValueFromRawData<int16_t>(value);
                if (size == sizeof(int32_t)) return getValueFromRawData<int32_t>(value);
                return getValueFromRawData<int64_t>(value);
            case ParameterTypes::FLOAT:
                if (size == sizeof(float)) return getValueFromRawData<float>(value);
                return getValueFromRawData<double>(value);
            case ParameterTypes::ASCII: {
                std::string str;
                std::ranges::copy(value, std::back_inserter(str));
                return str;
            }
            case ParameterTypes::RAW: {
                auto array = nlohmann::json::array();
                for (const auto &element: value) {
                    array.push_back(element);
                }
                return array;
            }
            case ParameterTypes::ERROR: {
                RfErrorCodes errorCode;
                memcpy(&errorCode, value.data(), sizeof(errorCode));
                return getStringFromRfErrorCode(errorCode);
            }
            case ParameterTypes::UNDEFINED:
            default:
                throw std::runtime_error("unsupported parameter type");
        }
    }


    std::vector<uint8_t> RfApi::Parameter::to_vector() const {
        std::vector<uint8_t> buffer;
        uint8_t specialByte = 0;

        if (value.size() == 0) {
            return buffer;
        }

        if (type == ParameterTypes::ASCII || type == ParameterTypes::RAW) {
            buffer.reserve(value.size() + ((value.size() / 16) + 1));

            for (size_t offset = 0; offset < value.size(); offset += 16) {
                const size_t chunkSize = std::min<size_t>(16, value.size() - offset);
                specialByte = getSpecialByte(static_cast<uint8_t>(type), static_cast<uint8_t>(chunkSize - 1));

                buffer.push_back(specialByte);
                buffer.insert(buffer.end(),
                              value.begin() + offset,
                              value.begin() + offset + chunkSize);
            }
        } else {
            buffer.reserve(value.size() + 1);
            specialByte = getSpecialByte(static_cast<uint8_t>(type), static_cast<uint8_t>(value.size() - 1));

            buffer.push_back(specialByte);
            buffer.insert(buffer.end(), value.begin(), value.end());
        }

        return buffer;
    }

    RfApi::RfCommand::RfCommand(std::vector<uint8_t> rawData) {
        constexpr uint8_t paramIndexOffset = 1; // Param with index 0 is reserved for UID / Notification type
        size_t rawDataOffset = 0;

        // Helper lambda, throws error on insufficient byte amount
        const auto requireBytes = [&rawDataOffset, &rawData](const size_t n) {
            if (rawDataOffset + n > rawData.size()) {
                throw std::runtime_error("unexpected end of data");
            }
        };

        // Parse command
        requireBytes(1);
        const auto [command, numOfParameters] = readSpecialByte(rawData[rawDataOffset++]);
        commandType = static_cast<CommandTypes>(command);

        // Return if command is not expecting params
        if (numOfParameters == 0) return;

        // Check first param for error
        requireBytes(1);
        const auto [firstParamRawType,_] = readSpecialByte(rawData[rawDataOffset]);
        if (static_cast<ParameterTypes>(firstParamRawType) == ParameterTypes::ERROR) {
            requireBytes(2);
            const auto errorCode = static_cast<RfErrorCodes>(rawData[rawDataOffset + 2]);
            parameters.emplace_back(errorCode);
            return;
        }

        // Parsee UID or Notification type
        if (commandType == CommandTypes::REPING || commandType == CommandTypes::RESPONSE) {
            requireBytes(1);
            const auto [uidParameterRawType, uidParameterLength] = readSpecialByte(rawData[rawDataOffset++]);
            if (static_cast<ParameterTypes>(uidParameterRawType) != ParameterTypes::UINT) {
                throw std::runtime_error("unexpected parameter type for UID");
            }
            requireBytes(uidParameterLength);


            size_t uid;
            std::array<uint8_t, sizeof(uid)> buffer{};
            std::copy_n(rawData.begin() + rawDataOffset, uidParameterLength, buffer.end() - uidParameterLength);
            uid = std::bit_cast<size_t>(buffer);

            // Swap endian for raw data values that are received in big endian
            if constexpr (std::endian::native == std::endian::little) {
                uid = std::byteswap(uid);
            }

            requestId.emplace(uid);
            rawDataOffset += uidParameterLength;
        } else if (commandType == CommandTypes::NOTIFY) {
            requireBytes(1);
            const auto [notifTypeParameterRawType, notifTypeParameterLength] =
                    readSpecialByte(rawData[rawDataOffset++]);

            if (static_cast<ParameterTypes>(notifTypeParameterRawType) != ParameterTypes::UINT) {
                throw std::runtime_error("unexpected parameter type for Notification Type");
            }
            if (notifTypeParameterLength != sizeof(uint8_t)) {
                throw std::runtime_error("unexpected parameter length for Notification Type");
            }
            requireBytes(notifTypeParameterLength);

            requestType.emplace(std::bit_cast<NotificationTypes>(rawData[rawDataOffset]));
            rawDataOffset += notifTypeParameterLength;
        }

        for (uint8_t i = paramIndexOffset; i < numOfParameters; ++i) {
            requireBytes(1);
            const auto [parameterRawType, parameterLength] = readSpecialByte(rawData[rawDataOffset++]);
            const auto parameterType = static_cast<ParameterTypes>(parameterRawType);
            requireBytes(parameterLength);

            Parameter parameter;
            const auto parameterData = std::span(rawData.data() + rawDataOffset, parameterLength);
            rawDataOffset += parameterLength;

            switch (parameterType) {
                case ParameterTypes::UINT:
                    copyRawDataToParameter<uint64_t>(parameter, parameterData);
                    break;
                case ParameterTypes::INT:
                    copyRawDataToParameter<int64_t>(parameter, parameterData);
                    break;
                case ParameterTypes::FLOAT:
                    copyRawDataToParameter<double>(parameter, parameterData);
                    break;
                case ParameterTypes::ASCII:
                    assignRawDataToParameter<std::string>(parameter, parameterData);
                    break;
                case ParameterTypes::RAW:
                    assignRawDataToParameter<std::vector<uint8_t> >(parameter, parameterData);
                    break;
                case ParameterTypes::ERROR:
                    copyRawDataToParameter<RfErrorCodes>(parameter, parameterData);
                    break;
                default:
                    throw std::runtime_error("unsupported parameter type");
            }

            parameters.push_back(parameter);
        }
    }

    std::vector<uint8_t> RfApi::RfCommand::to_vector() const {
        std::vector<uint8_t> buffer;
        const uint8_t specialByte = getSpecialByte(static_cast<uint8_t>(commandType),
                                                   static_cast<uint8_t>(parameters.size()));
        buffer.push_back(specialByte);


        std::vector<uint8_t> requestIdVector;
        if (requestId.has_value()) {
            requestIdVector = Parameter(static_cast<uint64_t>(requestId.value())).to_vector();;
        }

        std::vector<uint8_t> typeVector;

        if (requestType.has_value()) {
            uint8_t type = 0;

            switch (commandType) {
                case CommandTypes::GET:
                    type = static_cast<uint8_t>(std::get<GetTypes>(requestType.value()));
                    break;
                case CommandTypes::SET:
                    type = static_cast<uint8_t>(std::get<SetTypes>(requestType.value()));
                    break;
                case CommandTypes::NOTIFY:
                    type = static_cast<uint8_t>(std::get<NotificationTypes>(requestType.value()));
                    break;
                default: break;
            }
            typeVector = Parameter(static_cast<uint64_t>(type)).to_vector();
        }


        if (requestIdVector.size() > 0) {
            buffer.insert(buffer.end(), requestIdVector.begin(), requestIdVector.end());
        }

        if (typeVector.size() > 0) {
            buffer.insert(buffer.end(), typeVector.begin(), typeVector.end());
        }

        for (const auto &param: parameters) {
            const auto paramVector = param.to_vector();
            buffer.insert(buffer.end(), paramVector.begin(), paramVector.end());
        }

        return buffer;
    }

    std::string_view RfApi::getStringFromRfErrorCode(RfErrorCodes code) {
        switch (code) {
            case RfErrorCodes::UNKNOWN:
                return "Unknown error";
            default:
                return "Undefined error";
        }
    }

    RfApi::GetTypes RfApi::getTypeFromString(const std::string_view value) {
        static const std::unordered_map<std::string_view, GetTypes> strToGetMap{
            {msSENSOR_VALUE_STRING, GetTypes::SENSOR_VALUE},
            {msCONFIG_OPTION_STRING, GetTypes::CONFIG_OPTION},
            {msSENSOR_LIST_STRING, GetTypes::SENSOR_LIST},
            {msLOGS_STRING, GetTypes::LOGS},
            {msBATTERY_LEVEL_STRING, GetTypes::BATTERY_LEVEL}
        };

        const auto iter = strToGetMap.find(boost::algorithm::to_lower_copy(std::string(value)));
        return (iter != strToGetMap.end()) ? iter->second : GetTypes::UNDEFINED;
    }

    RfApi::SetTypes RfApi::setTypeFromString(const std::string_view value) {
        static const std::unordered_map<std::string_view, SetTypes> strToSetMap{
            {msCONFIG_OPTION_STRING, SetTypes::CONFIG_OPTION},
            {msTOGGLE_ACTUATOR_STRING, SetTypes::TOGGLE_ACTUATOR},
            {msSET_ACTUATOR_VALUE_STRING, SetTypes::SET_ACTUATOR_VALUE}
        };

        const auto iter = strToSetMap.find(boost::algorithm::to_lower_copy(std::string(value)));
        return (iter != strToSetMap.end()) ? iter->second : SetTypes::UNDEFINED;
    }

    RfApi::NotificationTypes RfApi::notificationTypeFromString(const std::string_view value) {
        static const std::unordered_map<std::string_view, NotificationTypes> strToNotifMap{
            {msMANUAL_TRIGGER_STRING, NotificationTypes::MANUAL_TRIGGER},
            {msPOWER_LOSS_STRING, NotificationTypes::POWER_LOSS},
            {msALERT_STRING, NotificationTypes::ALERT},
            {msWAKE_STRING, NotificationTypes::WAKE}
        };

        const auto iter = strToNotifMap.find(boost::algorithm::to_lower_copy(std::string(value)));
        return (iter != strToNotifMap.end()) ? iter->second : NotificationTypes::UNDEFINED;
    }

    std::string_view RfApi::notificationTypeToString(NotificationTypes value) {
        switch (value) {
            case NotificationTypes::MANUAL_TRIGGER:
                return msMANUAL_TRIGGER_STRING;
            case NotificationTypes::POWER_LOSS:
                return msPOWER_LOSS_STRING;
            case NotificationTypes::ALERT:
                return msALERT_STRING;
            case NotificationTypes::WAKE:
                return msWAKE_STRING;
            default:
                return msUNDEFINED_STRING;
        }
    }

    RfApi::RfCommand RfApi::toRfCommand(const SmartHome::API::ApiRequest &apiRequest) {
        RfCommand rfCommand;

        if (!apiRequest.params.has_value()) {
            throw std::invalid_argument("No parameters specified");
        }
        const auto &params = apiRequest.params.value();
        if (!(params.contains(sj::ParamsKeys::TARGET) && params.at(sj::ParamsKeys::TARGET) == msMEDIATOR_STRING)) {
            throw std::invalid_argument("Missing or invalid target parameter");
        }

        std::unique_ptr<nlohmann::json> methodParams;
        if (params.contains(sj::ParamsKeys::METHOD_PARAMS)) {
            methodParams = make_unique<nlohmann::json>(params[sj::ParamsKeys::METHOD_PARAMS]);
        }

        const auto method = boost::algorithm::to_lower_copy(apiRequest.method);

        if (apiRequest.id.hasValue()) {
            if (method == msSET_STRING && methodParams != nullptr && methodParams->is_array() &&
                methodParams->size() > 1) {
                rfCommand.commandType = CommandTypes::SET;
                // Read set type from first index
                rfCommand.requestType.emplace(setTypeFromString(methodParams.get()[0].get<std::string>()));
            } else if (method == msGET_STRING && methodParams != nullptr & methodParams->is_array() &&
                       methodParams->size() > 0) {
                rfCommand.commandType = CommandTypes::GET;
                // Read get type from first index
                rfCommand.requestType.emplace(getTypeFromString(methodParams.get()[0].get<std::string>()));
            } else if (method == msEXECUTE_STRING && methodParams != nullptr && methodParams->is_array() &&
                       methodParams->size() == 2) {
                const auto &action = methodParams.get()[0].get<std::string>();

                if (action == "sleep") rfCommand.commandType = CommandTypes::SLEEP;
                else if (action == "deep_sleep") rfCommand.commandType = CommandTypes::DEEP_SLEEP;
                else {
                    throw std::invalid_argument("Invalid action parameter specified");
                }
            } else if (method == msPING_STRING) {
                rfCommand.commandType = CommandTypes::PING;
            } else {
                throw std::invalid_argument("Invalid method specified");
            }

            // Pass remaining method params to rfCommand parameter vector
            for (int i = 1; i < methodParams->size(); i++) {
                rfCommand.parameters.push_back(Parameter::parameterFromJson(methodParams.get()[i]));
            }

            rfCommand.requestId = apiRequest.id.value();
        } else if (apiRequest.id.isUndefined()) {
            if (method == msNOTIFY_STRING) {
                rfCommand.commandType = CommandTypes::NOTIFY;
                // Read get notify from first index
                rfCommand.requestType.emplace(notificationTypeFromString(methodParams.get()[0].get<std::string>()));
            } else {
                throw std::invalid_argument("API notification is not of notify method");
            }
        }

        return rfCommand;
    }

    std::string RfApi::toApiString(RfCommand rfCommand) {
        if (rfCommand.commandType == CommandTypes::RESPONSE) {
            SmartHome::API::ApiResponse response;
            if (rfCommand.requestId.has_value()) {
                response.id = rfCommand.requestId.value();
            } else {
                // Set response id as null when command id is not set (error response)
                response.id = nullptr;
            }

            if (rfCommand.parameters.size() > 0) {
                // Check for error
                if (rfCommand.parameters[0].type == ParameterTypes::ERROR) {
                    // Return MODULE_RUNTIME_ERROR if error occurred on module side
                    SmartHome::API::ApiError error;
                    error.code = SmartHome::API::ErrorCodes::MODULE_RUNTIME_ERROR;
                    error.message = SmartHome::API::errorCodeToString(error.code);

                    RfErrorCodes rfErrorCode;
                    // TODO replace memcpy with copy_n and bit_cast
                    memcpy(&rfErrorCode, rfCommand.parameters[0].value.data(), rfCommand.parameters[0].value.size());
                    error.data = getStringFromRfErrorCode(rfErrorCode);
                    response.error = error;
                } else {
                    // Store response parameters inside result as an array
                    auto paramJsonArray = nlohmann::json::array();
                    for (const auto &param: rfCommand.parameters) {
                        paramJsonArray.push_back(param.parameterToJson());
                    }
                    response.result = paramJsonArray;
                }
            } else {
                // Return INTERNAL_ERROR if response has no parameters
                SmartHome::API::ApiError error;
                error.code = SmartHome::API::ErrorCodes::INTERNAL_ERROR;
                error.message = SmartHome::API::errorCodeToString(error.code);
                error.data = "Module response without values";

                response.error = error;
            }
            // Return response
            return response.to_string();
        }
        if (rfCommand.commandType == CommandTypes::NOTIFY) {
            if (!rfCommand.requestType.has_value()) {
                throw std::invalid_argument("RfCommand of notify type does not have NotificationType set");
            }

            SmartHome::API::ApiRequest notify;
            auto &params = notify.params.value();

            sa::InternalApi::Method method(sa::InternalApi::MethodTypes::NOTIFY);
            notify.method = method.to_string();

            sa::InternalApi::Target target(sa::InternalApi::TargetTypes::CORE);
            params[sj::ParamsKeys::TARGET] = target.to_string();

            auto notificationType = std::get<NotificationTypes>(rfCommand.requestType.value());
            params[sj::ParamsKeys::METHOD_PARAMS] = notificationTypeToString(notificationType);

            return notify.to_string();
        }

        // TODO !pr remove throws - replace them with error ApiResponses
        throw std::invalid_argument("Invalid command");
    }

    RfApi::RfApi(const std::shared_ptr<RfClient> &pRfClient) : mpRfClient(pRfClient) {
    }

    // void RfApi::handleOutgoing(SmartHome::connectionId_t connectionId, std::string &&message) {
    //     const std::string resultMessage = message;
    //     //TODO !pr result message is either JSON-RPC response/notification or a batch
    //     mpApiClient->send(resultMessage);
    // }

    void RfApi::handleIncoming(SmartHome::connectionId_t connectionId, std::string &&message) {
        const auto &pLogger = Mediator::Instance().mpLogger;
        sa::ApiRequest request;


        // TODO !pr przyjmowanie batch requestów (i pojedyńczych), modyfikacja sesji do obsługi batch requestów
        //  (batch requesty będą przsyłały klika rzeczy w jednej sesji np request i sleep)

        try {
            request(messageCopy);
        } catch (...) {
            pLogger->error("[RF_API] Failed to parse incoming message");
            return;
        }

        if (!request.params.has_value() && request.params->contains(sj::ParamsKeys::METHOD_PARAMS)) {
            pLogger->error("[RF_API] Request has no params");
            return;
        }

        auto &params = request.params.value()[sj::ParamsKeys::METHOD_PARAMS];
        // params must have at least module info struct and command type
        if (!params.is_array() && params.size() < 2) {
            pLogger->error("[RF_API] Request has invalid params");
            return;
        }

        const bool requestHasId = request.id.hasValue();

        Session::Metadata metadata;

        try {
            // TODO !pr change
            metadata = {
                .rfChannel = params[0]["module_rf_channel"].get<uint8_t>(),
                .targetLogicAddress = params[0]["module_id"].get<uint8_t>(),
                .command = params[1].get<std::string>(),
            };

            if (requestHasId) {
                metadata.sessionType = Session::Type::REQUEST;
                metadata.requestId = request.id.value();
            } else {
                metadata.sessionType = Session::Type::NOTIFICATION;
            }

            std::string tmpMetaParams;

            for (int i = 2; i < params.size(); i++) {
                tmpMetaParams += params[i].get<std::string>() + " ";
            }

            if (!tmpMetaParams.empty()) metadata.parameters = tmpMetaParams;
        } catch (const std::exception &e) {
            pLogger->errorf("[RF_API] Failed to convert API request to Rf session", e.what());
            return;
        }


        mpRfClient->addSession(std::move(metadata));
    }

    uint8_t getSpecialByte(const uint8_t firstHalf, const uint8_t secondHalf) {
        return firstHalf << 4 | secondHalf;
    }

    std::pair<uint8_t, uint8_t> readSpecialByte(const uint8_t specialByte) {
        static constexpr uint8_t mask = 0xF;
        std::pair<uint8_t, uint8_t> result;

        result.second = specialByte & mask;
        result.first = specialByte >> 4;
        return result;
    }
}
