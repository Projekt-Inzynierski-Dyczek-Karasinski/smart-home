#pragma once

#include <variant>
#include <string_view>

namespace Comms::API {
    /// @brief Low-level (1-4) and high-level (5-12) command identifiers used in the communication protocol.
    enum class commandTypes : uint8_t {
        UNKNOWN = 0,
        ACKNOWLEDGE = 1,
        NEGATIVE = 2,
        REPEAT = 3,
        END = 4,
        PING = 5,
        REPING = 6,
        SLEEP = 7,
        DEEP_SLEEP = 8,
        GET = 9,
        SET = 10,
        RESPONSE = 11,
        NOTIFY = 12
    };

    /// @brief Error codes returned by the API to describe why a request failed.
    enum class errorTypes : uint8_t {
        UNKNOWN = 0,
        BAD_COMMAND = 1,
        UNKNOWN_COMMAND = 2,
        BAD_ARGUMENT = 3,
        NOT_IMPLEMENTED = 4,
        INTERNAL_ERROR = 5,
    };

    /// @brief GET request subtypes that specify what information should be read from the device.
    enum class getTypes : uint8_t {
        UNKNOWN = 0,
        SENSOR_VALUE = 1,
        CONFIG_VALUE = 2,
        SENSOR_LIST = 3,
        LOGS = 4,
        BATTERY_STATE = 5,
        SENSOR_VALUE_WITH_FORCE_NEW_READING = 6,
        ACTUATOR_STATE = 7
    };

    /// @brief SET request subtypes that specify what action the module should perform.
    enum class setTypes : uint8_t {
        UNKNOWN = 0,
        CHANGE_CONFIG = 1,
        ACTUATOR_OPERATION = 2,
        ACTUATOR_TOGGLE = 3,
        OTA = 4
    };

    /// @brief Notification event types to report important events.
    enum class notifyTypes : uint8_t {
        UNKNOWN = 0,
        MANUAL_WAKE_UP = 1,
        POWER_LOSS = 2,
        SENSOR_ALERT = 3,
        MODULE_WAKE_UP = 4
    };

    /// @brief Parameter types used to describe the format of a transmitted value.
    enum class parametersTypes : uint8_t {
        UNKNOWN = 0,
        UINT = 1,
        INT = 2,
        FLOAT = 3,
        ASCII = 4,
        RAW = 5,
        ERROR = 6
    };

    inline std::string_view parametersTypesToString(const parametersTypes parameterType) {
        switch (parameterType) {
            case parametersTypes::INT:   return "INT";
            case parametersTypes::UINT:  return "UINT";
            case parametersTypes::FLOAT: return "FLOAT";
            case parametersTypes::ASCII: return "ASCII";
            case parametersTypes::RAW:   return "RAW";
            case parametersTypes::ERROR: return "ERROR";
            default:                     return "UNKNOWN";
        }
    }

    using parameterVariant = std::variant<
        std::monostate,
        uint8_t,
        uint16_t,
        uint32_t,
        uint64_t,
        int8_t,
        int16_t,
        int32_t,
        int64_t,
        float,
        double,
        const char*,
        const uint8_t*
    >;

    /// @brief Enumerates the concrete C++ types that can be stored in parameterVariant for serialization/deserialization.
    enum class ParameterVariantEnum : uint8_t {
        MONOSTATE = 0,
        UINT8  = 1,
        UINT16 = 2,
        UINT32 = 3,
        UINT64 = 4,
        INT8   = 5,
        INT16  = 6,
        INT32  = 7,
        INT64  = 8,
        FLOAT  = 9,
        DOUBLE = 10,
        ASCII  = 11,
        RAW  = 12
    };
}