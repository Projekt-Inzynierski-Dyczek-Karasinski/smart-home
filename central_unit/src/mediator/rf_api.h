#pragma once

#include "api.h"
#include "api_client.h"
#include "../core/api/internal_api.h"

#include <boost/algorithm/string/case_conv.hpp>

namespace sa = SmartHome::API;

namespace SmartHomeMediator {
    class RfClient;

    class RfApi final : public sa::Api {
    public:
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

        static std::string_view getStringFromRfErrorCode(RfErrorCodes code);

        static GetTypes getTypeFromString(std::string_view value);

        static SetTypes setTypeFromString(std::string_view value);

        static NotificationTypes notificationTypeFromString(std::string_view value);

        static std::string_view notificationTypeToString(NotificationTypes value);

        static RfCommand toRfCommand(const SmartHome::API::ApiRequest &apiRequest);

        static std::string toApiString(RfCommand rfCommand);

        RfApi(const std::shared_ptr<RfClient> &pRfClient);

        // void handleOutgoing(SmartHome::connectionId_t connectionId, std::string &&message) override;

        void handleIncoming(SmartHome::connectionId_t connectionId, std::string &&message) override;

    private:
        std::shared_ptr<RfClient> mpRfClient;
        // std::shared_ptr<ApiClient> mpApiClient;

        static constexpr auto msMAX_PARAMETERS = 16;
        static constexpr auto msMAX_COMMANDS = msMAX_PARAMETERS;

        // Target string
        static constexpr std::string_view msMEDIATOR_STRING = "mediator";

        // Commands strings
        static constexpr std::string_view msGET_STRING = "get";
        static constexpr std::string_view msSET_STRING = "set";
        static constexpr std::string_view msNOTIFY_STRING = "notify";
        static constexpr std::string_view msEXECUTE_STRING = "execute";
        static constexpr std::string_view msPING_STRING = "ping";

        // Set/Get/Notify type strings
        static constexpr std::string_view msCONFIG_OPTION_STRING = "config_option";
        static constexpr std::string_view msTOGGLE_ACTUATOR_STRING = "toggle_actuator";
        static constexpr std::string_view msSET_ACTUATOR_VALUE_STRING = "set_actuator_value";
        static constexpr std::string_view msSENSOR_VALUE_STRING = "sensor_value";
        static constexpr std::string_view msSENSOR_LIST_STRING = "sensor_list";
        static constexpr std::string_view msLOGS_STRING = "logs";
        static constexpr std::string_view msBATTERY_LEVEL_STRING = "battery_level";
        static constexpr std::string_view msMANUAL_TRIGGER_STRING = "manual_trigger";
        static constexpr std::string_view msPOWER_LOSS_STRING = "power_loss";
        static constexpr std::string_view msALERT_STRING = "alert";
        static constexpr std::string_view msWAKE_STRING = "wake";


        static constexpr std::string_view msUNDEFINED_STRING = "undefined";
    };

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
    void copyRawDataToParameter(RfApi::Parameter &param, const std::span<uint8_t> &rawData) {
        if (rawData.size() > sizeof(T)) throw std::runtime_error("rawData too long for type");

        std::array<uint8_t, sizeof(T)> buffer{};
        std::copy_n(rawData.begin(), rawData.size(), buffer.end() - rawData.size());
        T value = std::bit_cast<T>(buffer);

        param = RfApi::Parameter(value);
    }

    template<typename T>
    void assignRawDataToParameter(RfApi::Parameter &param, const std::span<uint8_t> &rawData) {
        T value{};
        value.assign(rawData.begin(), rawData.end());
        param = RfApi::Parameter(value);
    }
}
