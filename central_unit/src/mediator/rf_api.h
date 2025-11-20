#pragma once
#include "api.h"
#include "api_client.h"
#include "rf_client.h"

namespace sa = SmartHome::API;

namespace SmartHomeMediator {
    class RfApi final : public sa::Api {
    public:
        enum class RfErrorCodes: uint8_t {
            UNKNOWN = 0,
            //TODO add error codes
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
            END,
            // High level (Handled/executed by target device)
            PING,
            REPING,
            SLEEP,
            DEEP_SLEEP,
            GET,
            SET,
            RESPONSE,
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
            ParameterTypes type;
            std::vector <uint8_t> value;

            explicit Parameter(uint64_t newValue);

            explicit Parameter(int64_t newValue);

            explicit Parameter(double newValue);

            explicit Parameter(std::string newValue);

            explicit Parameter(std::vector<uint8_t> newValue);

            explicit Parameter(RfErrorCodes newValue);
        };

        class RfCommand {
        public:
            CommandTypes command{};
            // command == NOTIFY
            NotificationTypes notifType{};
            // command == GET || SET
            SmartHome::apiId_t requestId{};
            // command == GET
            GetTypes getType{};
            // command == SET
            SetTypes setType{};
            std::vector<Parameter> parameters{};
        };


        RfCommand toRfCommand(SmartHome::API::ApiRequest apiRequest);

        std::string toApiString(RfCommand rfCommand);


        RfApi(const std::shared_ptr<RfClient> &pRfClient, const std::shared_ptr<ApiClient> &pApiClient);

        void handleOutgoing(SmartHome::connectionId_t connectionId, std::string &&message) override;

        void handleIncoming(SmartHome::connectionId_t connectionId, std::string &&message) override;

    private:
        std::shared_ptr<RfClient> mpRfClient;
        std::shared_ptr<ApiClient> mpApiClient;

        static constexpr auto msMAX_PARAMETERS = 16;
        static constexpr auto msMAX_COMMANDS = msMAX_PARAMETERS;

        static constexpr std::string_view msMEDIATOR_STRING = "mediator";
        static constexpr std::string_view msGET_STRING = "get";
        static constexpr std::string_view msSET_STRING = "set";
        static constexpr std::string_view msNOTIFY_STRING = "notify";
        static constexpr std::string_view msEXECUTE_STRING = "execute";
        static constexpr std::string_view msPING_STRING = "ping";
    };
}
