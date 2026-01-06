#pragma once
#include "api.h"

#include <vector>
#include <optional>
#include <variant>

#include <nlohmann/json.hpp>


namespace SmartHomeMediator::RfTypes {
    class RfCommand;

    enum class RfErrorCodes: uint8_t {
        UNKNOWN = 0,
        BAD_COMMAND,
        BAD_ARGUMENT,
        INTERNAL_ERROR
    };

    enum class ParameterTypes: uint8_t {
        UNDEFINED = 0,
        // Regular types
        UINT,
        INT,
        FLOAT,
        ASCII,
        // Special types
        RAW,
        ERROR
    };

    enum class CommandTypes: uint8_t {
        UNDEFINED = 0,
        // Low level (Used in rf protocol)
        ACKNOWLEDGE,
        NEGATIVE,
        REPEAT,
        END, // Only sent
        // High level (Handled/executed by target device)
        PING,
        REPING, // Only received
        SLEEP,
        DEEP_SLEEP,
        GET,
        SET,
        RESPONSE, // Only received
        NOTIFY
    };

    enum class GetTypes: uint8_t {
        UNDEFINED = 0,
        // Sensor values
        SENSOR_VALUE,
        // Config values
        CONFIG_OPTION,
        SENSOR_LIST,
        // Debug/Status values
        LOGS,
        BATTERY_LEVEL
    };

    enum class SetTypes: uint8_t {
        UNDEFINED = 0,
        // Config values
        CONFIG_OPTION,
        // Actuator values
        TOGGLE_ACTUATOR,
        SET_ACTUATOR_VALUE
    };

    enum class NotificationTypes: uint8_t {
        UNDEFINED = 0,
        // Received from modules
        MANUAL_TRIGGER,
        POWER_LOSS,
        ALERT,
        // Send to modules
        WAKE
    };

    enum class SessionType : uint8_t {
        FROM_CENTRAL_UNIT,
        FROM_MODULE
    };

    struct SessionMetadata {
        SessionType sessionType = SessionType::FROM_CENTRAL_UNIT;
        uint8_t rfChannel{};
        uint8_t targetLogicAddress{};
        std::vector<RfCommand> commands{};
    };

    // Target string
    inline constexpr std::string_view MEDIATOR_STRING = "module_mediator";

    // Commands strings
    inline constexpr std::string_view GET_STRING = "get";
    inline constexpr std::string_view SET_STRING = "set";
    inline constexpr std::string_view NOTIFY_STRING = "notify";
    inline constexpr std::string_view EXECUTE_STRING = "execute";
    inline constexpr std::string_view PING_STRING = "ping";

    // Set/Get/Notify type strings
    inline constexpr std::string_view CONFIG_OPTION_STRING = "config_option";
    inline constexpr std::string_view TOGGLE_ACTUATOR_STRING = "toggle_actuator";
    inline constexpr std::string_view SET_ACTUATOR_VALUE_STRING = "set_actuator_value";
    inline constexpr std::string_view SENSOR_VALUE_STRING = "sensor_value";
    inline constexpr std::string_view SENSOR_LIST_STRING = "sensor_list";
    inline constexpr std::string_view LOGS_STRING = "logs";
    inline constexpr std::string_view BATTERY_LEVEL_STRING = "battery_level";
    inline constexpr std::string_view MANUAL_TRIGGER_STRING = "manual_trigger";
    inline constexpr std::string_view POWER_LOSS_STRING = "power_loss";
    inline constexpr std::string_view ALERT_STRING = "alert";
    inline constexpr std::string_view WAKE_STRING = "wake";

    // Action types strings
    inline constexpr std::string_view SLEEP_STRING = "sleep";
    inline constexpr std::string_view DEEP_SLEEP_STRING = "deep_sleep";


    inline constexpr std::string_view UNDEFINED_STRING = "undefined";

    struct Packet {
    private:
        static constexpr uint8_t msPAYLOAD_MAX_SIZE = 6;
        static constexpr uint16_t msCHECKSUM_MODULO = 256;
        static constexpr uint8_t msEND_MARKER = 0;
        static constexpr uint8_t msFILL_SYMBOL = 0;

    public:
        std::array<uint8_t, 6> macAddress{};
        uint8_t logicAddress{};
        uint8_t packetsLeft{};
        std::array<uint8_t, msPAYLOAD_MAX_SIZE> payload{};
        uint8_t checksum{};
        uint8_t endMarker{};

        static Packet from_bytes(std::span<const uint8_t> data);

        static Packet from_vector(const std::vector<uint8_t> &data);

        std::span<const uint8_t> as_bytes() const;

        std::vector<uint8_t> to_vector() const;

        bool isValid() const;

        bool isLastPacket() const;

        // std::vector<uint8_t> getPayload() const;

        static uint8_t getPayloadMaxSize();

        static uint8_t getEndMarker();

        static uint8_t getFillSymbol();

        void insertChecksum();

        void insertEndMarker();

    private:
        uint16_t static calculateChecksum(const Packet &packet);

        bool static verifyChecksum(const Packet &packet);
    } __attribute__((packed));


    struct Parameter {
        ParameterTypes type{};
        std::vector<uint8_t> value{};

        Parameter() = default;

        explicit Parameter(uint64_t newValue);

        explicit Parameter(int64_t newValue);

        explicit Parameter(double newValue);

        explicit Parameter(std::string_view newValue);

        explicit Parameter(const std::vector<uint8_t> &newValue);

        explicit Parameter(RfErrorCodes newValue);

        static Parameter parameterFromJson(const nlohmann::json &json);

        nlohmann::json parameterToJson() const;

        std::vector<uint8_t> to_vector() const;
    };

    class RfCommand {
    public:
        RfCommand() = default;

        explicit RfCommand(std::vector<uint8_t> rawData);

        CommandTypes commandType = CommandTypes::UNDEFINED;

        // TODO !pr move optionals into parameters
        std::optional<std::variant<GetTypes, SetTypes, NotificationTypes> > requestType;

        std::optional<SmartHome::apiId_t> requestId;

        std::vector<Parameter> parameters;

        std::vector<uint8_t> to_vector() const;
    };

    std::string_view getStringFromRfErrorCode(RfErrorCodes code);

    GetTypes getTypeFromString(std::string_view value);

    SetTypes setTypeFromString(std::string_view value);

    NotificationTypes notificationTypeFromString(std::string_view value);

    std::string_view notificationTypeToString(NotificationTypes value);

    uint8_t getSpecialByte(uint8_t firstHalf, uint8_t secondHalf);

    std::pair<uint8_t, uint8_t> readSpecialByte(uint8_t specialByte);

    template<typename T>
        requires std::is_integral_v<T>
    void assignSwappedEndian(std::vector<uint8_t> &buffer, T value) {
        const auto valueSize = sizeof(T);
        buffer.resize(valueSize);

        if constexpr (std::endian::native == std::endian::little) {
            auto swapped = std::byteswap(value);
            memcpy(buffer.data(), &swapped, valueSize);
        } else {
            memcpy(buffer.data(), &value, valueSize);
        }
    }

    template<typename T>
        requires std::is_floating_point_v<T>
    void assignSwappedEndian(std::vector<uint8_t> &buffer, T value) {
        const auto valueSize = sizeof(T);
        buffer.resize(sizeof(T));

        if constexpr (std::endian::native == std::endian::little) {
            if constexpr (std::is_same_v<T, float>) {
                auto raw = std::bit_cast<uint32_t>(value);
                raw = std::byteswap(raw);
                const auto swapped = std::bit_cast<float>(raw);
                memcpy(buffer.data(), &swapped, valueSize);
            } else {
                auto raw = std::bit_cast<uint64_t>(value);
                raw = std::byteswap(raw);
                const auto swapped = std::bit_cast<double>(raw);
                memcpy(buffer.data(), &swapped, valueSize);
            }
        } else {
            memcpy(buffer.data(), &value, valueSize);
        }
    }

    template<typename T>
    T getValueFromRawData(const std::vector<uint8_t> &rawValue) {
        const auto valueSize = sizeof(T);
        T value;
        memcpy(&value, &rawValue[0], valueSize);

        // if constexpr (std::endian::native == std::endian::little) {
        //     if constexpr (std::is_integral_v<T>) {
        //         value = std::byteswap(value);
        //         return value;
        //     }
        //     if constexpr (std::is_floating_point_v<T>) {
        //         if constexpr (std::is_same_v<T, float>) {
        //             auto raw = std::bit_cast<uint32_t>(value);
        //             raw = std::byteswap(raw);
        //             value = std::bit_cast<T>(raw);
        //         } else {
        //             auto raw = std::bit_cast<uint64_t>(value);
        //             raw = std::byteswap(raw);
        //             value = std::bit_cast<T>(raw);
        //         }
        //         return value;
        //     }
        //     throw std::invalid_argument("not supported type");
        // }

        return value;
    }

    template<typename T>
    void copyRawDataToParameter(RfTypes::Parameter &param, const std::span<uint8_t> &rawData) {
        if (rawData.size() > sizeof(T)) throw std::runtime_error("rawData too long for type");

        std::array<uint8_t, sizeof(T)> buffer{};
        std::copy_n(rawData.begin(), rawData.size(), buffer.end() - rawData.size());
        T value = std::bit_cast<T>(buffer);

        param = RfTypes::Parameter(value);
    }

    template<typename T>
    void assignRawDataToParameter(RfTypes::Parameter &param, const std::span<uint8_t> &rawData) {
        T value{};
        value.assign(rawData.begin(), rawData.end());
        param = RfTypes::Parameter(value);
    }
}
