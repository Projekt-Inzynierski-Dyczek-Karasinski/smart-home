#include "rf_types.h"

#include <boost/algorithm/string/case_conv.hpp>

namespace SmartHomeMediator::RfTypes {
    namespace sj = SmartHome::JsonRpcStrings;


    Packet Packet::from_bytes(const std::span<const uint8_t> data) {
        if (data.size() != sizeof(Packet)) {
            throw std::invalid_argument("Invalid size");
        }

        Packet packet{};
        std::memcpy(&packet, data.data(), data.size());
        return packet;
    }

    Packet Packet::from_vector(const std::vector<uint8_t> &data) {
        return from_bytes(data);
    }

    std::span<const uint8_t> Packet::as_bytes() const {
        return {reinterpret_cast<const uint8_t *>(this), sizeof(*this)};
    }

    std::vector<uint8_t> Packet::to_vector() const {
        auto bytes = as_bytes();
        return {bytes.begin(), bytes.end()};
    }

    bool Packet::isValid() const {
        return endMarker == msEND_MARKER && verifyChecksum(*this);
    }

    bool Packet::isLastPacket() const {
        return packetsLeft == 0;
    }

    // std::vector<uint8_t> Packet::getPayload() const {
    //     return {payload.begin(), payload.end()};
    // }

    uint8_t Packet::getPayloadMaxSize() {
        return msPAYLOAD_MAX_SIZE;
    }

    uint8_t Packet::getEndMarker() {
        return msEND_MARKER;
    }

    uint8_t Packet::getFillSymbol() {
        return msFILL_SYMBOL;
    }

    void Packet::insertChecksum() {
        checksum = 0;
        const auto tmpChecksum = calculateChecksum(*this);

        checksum = (msCHECKSUM_MODULO - (tmpChecksum % msCHECKSUM_MODULO)) % msCHECKSUM_MODULO;
    }

    void Packet::insertEndMarker() {
        endMarker = msEND_MARKER;
    }

    uint16_t Packet::calculateChecksum(const Packet &packet) {
        uint16_t tmpChecksum = 0;
        for (const auto &byte: packet.as_bytes()) {
            tmpChecksum += byte;
        }
        return tmpChecksum;
    }

    bool Packet::verifyChecksum(const Packet &packet) {
        return calculateChecksum(packet) % msCHECKSUM_MODULO == 0;
    }

    Parameter::Parameter(const uint64_t newValue) {
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

    Parameter::Parameter(const int64_t newValue) {
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

    Parameter::Parameter(const double newValue) {
        type = ParameterTypes::FLOAT;

        if (std::abs(newValue) <= std::numeric_limits<float>::max()) {
            // Check precision loss
            const double tolerance = 2.0 * std::numeric_limits<float>::epsilon() * std::abs(newValue);
            const auto floatValue = static_cast<float>(newValue);
            const auto doubleValue = static_cast<double>(floatValue);

            if (std::abs(doubleValue - newValue) <= tolerance) {
                assignSwappedEndian(value, floatValue);
                return;
            }
        }

        assignSwappedEndian(value, newValue);
    }

    Parameter::Parameter(const std::string_view newValue) {
        type = ParameterTypes::ASCII;

        value.assign(newValue.begin(), newValue.end());
    }

    Parameter::Parameter(const std::vector<uint8_t> &newValue) {
        type = ParameterTypes::RAW;

        value = newValue;
    }

    Parameter::Parameter(const RfErrorCodes newValue) {
        type = ParameterTypes::ERROR;

        assignSwappedEndian(value, static_cast<uint8_t>(newValue));
    }

    Parameter Parameter::parameterFromJson(const nlohmann::json &json) {
        if (json.is_number_unsigned()) return Parameter(json.get<uint64_t>());
        if (json.is_number_integer()) return Parameter(json.get<int64_t>());
        if (json.is_number_float()) return Parameter(json.get<double>());
        if (json.is_string()) return Parameter(json.get<std::string>());
        if (json.is_array()) return Parameter(json.get<std::vector<uint8_t> >());
        throw std::runtime_error("unsupported JSON parameter type");
    }

    nlohmann::json Parameter::parameterToJson() const {
        const size_t size = value.size();
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


    std::vector<uint8_t> Parameter::to_vector() const {
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

    RfCommand::RfCommand(std::vector<uint8_t> rawData) {
        constexpr uint8_t paramIndexOffset = 1; // Param with index 0 is reserved for UID / Notification type
        size_t rawDataOffset = 0;

        // Helper lambda, throws error on insufficient byte amount
        const auto requireBytes = [&rawDataOffset, &rawData](const size_t n) {
            if (rawDataOffset + n > rawData.size()) {
                throw std::runtime_error("unexpected end of data");
            }
        };

        // Helper lambda to adjust value of param length read from special byte.
        // Actual parameter length is equal to value contained in second half of special byte + 1
        const auto adjustParamLength = [](unsigned char &value) {
            value = value + 1;
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

        // Parse UID or Notification type
        if (commandType == CommandTypes::REPING || commandType == CommandTypes::RESPONSE) {
            requireBytes(1);
            auto [uidParameterRawType, uidParameterLength] = readSpecialByte(rawData[rawDataOffset++]);
            if (static_cast<ParameterTypes>(uidParameterRawType) != ParameterTypes::UINT) {
                throw std::runtime_error("unexpected parameter type for UID");
            }
            adjustParamLength(uidParameterLength);
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
            auto [notifTypeParameterRawType, notifTypeParameterLength] =
                    readSpecialByte(rawData[rawDataOffset++]);

            if (static_cast<ParameterTypes>(notifTypeParameterRawType) != ParameterTypes::UINT) {
                throw std::runtime_error("unexpected parameter type for Notification Type");
            }
            if (notifTypeParameterLength != sizeof(uint8_t)) {
                throw std::runtime_error("unexpected parameter length for Notification Type");
            }
            adjustParamLength(notifTypeParameterLength);
            requireBytes(notifTypeParameterLength);

            requestType.emplace(std::bit_cast<NotificationTypes>(rawData[rawDataOffset]));
            rawDataOffset += notifTypeParameterLength;
        }

        for (uint8_t i = paramIndexOffset; i < numOfParameters; i++) {
            requireBytes(1);
            auto [parameterRawType, parameterLength] = readSpecialByte(rawData[rawDataOffset++]);
            const auto parameterType = static_cast<ParameterTypes>(parameterRawType);
            adjustParamLength(parameterLength);
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
                    if (parameterLength == sizeof(float)) {
                        copyRawDataToParameter<float>(parameter, parameterData);
                    } else {
                        copyRawDataToParameter<double>(parameter, parameterData);
                    }
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
                    char buffer[64];
                    sprintf(buffer, "unexpected parameter type %d", parameterRawType);
                    throw std::runtime_error(buffer);
            }

            parameters.push_back(parameter);
        }
    }

    std::vector<uint8_t> RfCommand::to_vector() const {
        std::vector<uint8_t> requestIdVector;
        if (requestId.has_value()) {
            requestIdVector = Parameter(static_cast<uint64_t>(requestId.value())).to_vector();;
        }


        std::vector<uint8_t> buffer;
        const uint8_t specialByte = getSpecialByte(static_cast<uint8_t>(commandType),
                                                   static_cast<uint8_t>(parameters.size()) +
                                                   (requestId.has_value() ? 1 : 0) +
                                                   (requestType.has_value() ? 1 : 0));
        buffer.push_back(specialByte);


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


        if (!requestIdVector.empty()) {
            buffer.insert(buffer.end(), requestIdVector.begin(), requestIdVector.end());
        }

        if (!typeVector.empty()) {
            buffer.insert(buffer.end(), typeVector.begin(), typeVector.end());
        }

        for (const auto &param: parameters) {
            const auto paramVector = param.to_vector();
            buffer.insert(buffer.end(), paramVector.begin(), paramVector.end());
        }

        return buffer;
    }

    std::string_view getStringFromRfErrorCode(const RfErrorCodes code) {
        switch (code) {
            case RfErrorCodes::UNKNOWN:
                return "Unknown error";
            case RfErrorCodes::BAD_COMMAND:
                return "Bad command";
            case RfErrorCodes::UNKNOWN_COMMAND:
                return "Unknown command";
            case RfErrorCodes::BAD_ARGUMENT:
                return "Bad argument";
            case RfErrorCodes::NOT_IMPLEMENTED:
                return "Not implemented";
            case RfErrorCodes::INTERNAL_ERROR:
                return "Internal error";
            default:
                return "Undefined error";
        }
    }

    GetTypes getTypeFromString(const std::string_view value) {
        static const std::unordered_map<std::string_view, GetTypes> strToGetMap{
            {SENSOR_VALUE_STRING, GetTypes::SENSOR_VALUE},
            {CONFIG_OPTION_STRING, GetTypes::CONFIG_OPTION},
            {SENSOR_LIST_STRING, GetTypes::SENSOR_LIST},
            {LOGS_STRING, GetTypes::LOGS},
            {BATTERY_LEVEL_STRING, GetTypes::BATTERY_LEVEL},
            {FORCE_READ_SENSOR_VALUE_STRING, GetTypes::FORCE_READ_SENSOR_VALUE}
        };

        const auto iter = strToGetMap.find(boost::algorithm::to_lower_copy(std::string(value)));
        return (iter != strToGetMap.end()) ? iter->second : GetTypes::UNDEFINED;
    }

    SetTypes setTypeFromString(const std::string_view value) {
        static const std::unordered_map<std::string_view, SetTypes> strToSetMap{
            {CONFIG_OPTION_STRING, SetTypes::CONFIG_OPTION},
            {TOGGLE_ACTUATOR_STRING, SetTypes::TOGGLE_ACTUATOR},
            {SET_ACTUATOR_VALUE_STRING, SetTypes::SET_ACTUATOR_VALUE}
        };

        const auto iter = strToSetMap.find(boost::algorithm::to_lower_copy(std::string(value)));
        return (iter != strToSetMap.end()) ? iter->second : SetTypes::UNDEFINED;
    }

    NotificationTypes notificationTypeFromString(const std::string_view value) {
        static const std::unordered_map<std::string_view, NotificationTypes> strToNotifMap{
            {MANUAL_TRIGGER_STRING, NotificationTypes::MANUAL_TRIGGER},
            {POWER_LOSS_STRING, NotificationTypes::POWER_LOSS},
            {ALERT_STRING, NotificationTypes::ALERT},
            {WAKE_STRING, NotificationTypes::WAKE}
        };

        const auto iter = strToNotifMap.find(boost::algorithm::to_lower_copy(std::string(value)));
        return (iter != strToNotifMap.end()) ? iter->second : NotificationTypes::UNDEFINED;
    }

    std::string_view notificationTypeToString(const NotificationTypes value) {
        switch (value) {
            case NotificationTypes::MANUAL_TRIGGER:
                return MANUAL_TRIGGER_STRING;
            case NotificationTypes::POWER_LOSS:
                return POWER_LOSS_STRING;
            case NotificationTypes::ALERT:
                return ALERT_STRING;
            case NotificationTypes::WAKE:
                return WAKE_STRING;
            default:
                return UNDEFINED_STRING;
        }
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
