#pragma once

#include "rf_types.h"
#include "api.h"
#include "api_client.h"
#include "../core/api/internal_api.h"


namespace sa = SmartHome::API;

namespace SmartHomeMediator {
    class RfClient;
    class Session;

    class RfApi final : public sa::Api {
    public:
        static std::unique_ptr<RfTypes::Command> toRfCommand(
            const SmartHome::API::ApiRequest &apiRequest,
            bool isConfigCommand = false);

        static std::unique_ptr<RfTypes::MediatorConfigCommand> toConfigCommand(
            const SmartHome::API::ApiRequest &apiRequest, std::string_view method,
            const std::unique_ptr<nlohmann::json> &pMethodParams);

        static std::string toApiString(RfTypes::RfCommand rfCommand);

        explicit RfApi(const std::shared_ptr<RfClient> &pRfClient);

        void initialize(const std::function<void(const std::string &message)> &messageHandler);

        /**
         *
         * @param connectionId
         * @param message
         *
         * @note Message must be in JSON RPC format with `params` field formated as follows:
         *       \code{.json}
         *       {
         *         "target": "module_mediator",
         *         "module_info": {
         *           "logic_address": <uint>,
         *           "rf_channel": <uint>
         *         },
         *         "method_params": []  // Array of positional parameters for the specific method
         *       }
         *       \endcode
         */
        void handleIncoming(SmartHome::connectionId_t connectionId, std::string &&message) override;

        void handleOutgoing(SmartHome::connectionId_t connectionId, std::string &&message) override;

    private:
        std::shared_ptr<RfClient> mpRfClient;
        std::function<void(const std::string &message)> mMessageHandler;

        static constexpr auto msMAX_PARAMETERS = 16;
        static constexpr auto msMAX_COMMANDS = msMAX_PARAMETERS;

        static RfTypes::SessionMetadata toMetadata(const SmartHome::API::ApiRequest &apiRequest);
    };
}
