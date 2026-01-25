#pragma once

#include "rf_types.h"
#include "api.h"
#include "../../core/api/internal_api.h"


namespace sa = SmartHome::API;

namespace SmartHomeMediator {
    class RfClient;
    class Session;

    /**
     * @brief JSON-RPC API handler for RF module communication.
     *
     * @details Translates between JSON-RPC API messages and internal RF command structures.
     *          Validates message format and converts between API and RF protocol representations.
     */
    class RfApi final : public sa::Api {
    public:
        /**
         * @brief Construct RF API handler.
         *
         * @param pRfClient Shared pointer to RF client for command execution.
         */
        explicit RfApi(const std::shared_ptr<RfClient> &pRfClient) : mpRfClient(pRfClient) {
        }

        /**
         * @brief Initialize message handling.
         *
         * @param messageHandler Callback function invoked for outgoing messages.
         *
         * @note Must be called before processing any messages.
         */
        void initialize(const std::function<void(const std::string &message)> &messageHandler);

        /**
         * @brief Handle incoming API message from SmartHome core.
         *
         * @details Parses JSON-RPC message, validates format, converts to RF command,
         *          and forwards to RF client for execution.
         *
         * @param connectionId Connection identifier for response routing.
         * @param message JSON-RPC formatted message string.
         *
         * @note Message must contain `params` field formatted as:
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

        /**
         * @brief Handle outgoing message to SmartHome core.
         *
         * @details Routes message through registered message handler callback.
         *
         * @param connectionId Connection identifier.
         * @param message Message string to send.
         */
        void handleOutgoing(SmartHome::connectionId_t connectionId, std::string &&message) override;

        /**
         * @brief Convert RF command to SmartHome::API JSON-RPC format.
         *
         * @param rfCommand RF command type.
         *
         * @return SmartHome::API JSON-RPC formated string.
         */
        static std::string toApiString(RfTypes::RfCommand rfCommand);

    private:
        /**
         * @brief Convert API request to internal RF command structure.
         *
         * @param apiRequest Parsed JSON-RPC API request.
         * @param isConfigCommand true if command targets mediator configuration.
         *
         * @return Unique pointer to RF command, or nullptr on parse error.
         *
         * @throws std::exception on validation or conversion errors.
         */
        static std::unique_ptr<RfTypes::Command> toRfCommand(
            const SmartHome::API::ApiRequest &apiRequest,
            bool isConfigCommand = false);

        /**
         * @brief Convert API request to mediator configuration command.
         *
         * @param apiRequest Parsed JSON-RPC API request.
         * @param method API method name.
         * @param pMethodParams Pointer to method parameters JSON object.
         *
         * @return Unique pointer to mediator config command, or nullptr on parse error.
         *
         * @throws std::exception on validation or conversion errors.
         */
        static std::unique_ptr<RfTypes::MediatorConfigCommand> toConfigCommand(
            const SmartHome::API::ApiRequest &apiRequest,
            std::string_view method,
            const std::unique_ptr<nlohmann::json> &pMethodParams);

        /**
          * @brief Parse API request into RF command structure.
          *
          * @details Validates method name, extracts parameters, and populates RF command fields.
          *
          * @param pCommand Pointer to RF command to populate.
          * @param method API method name.
          * @param pMethodParams Pointer to method parameters JSON array.
          *
          * @throws std::exception via throwParseError on validation failures.
          */
        static void parseRequest(RfTypes::RfCommand *pCommand, std::string_view method, nlohmann::json *pMethodParams);

        /**
         * @brief Parse API notification into RF command structure.
         *
         * @details Similar to parseRequest but for notification messages (no response expected).
         *
         * @param pCommand Pointer to RF command to populate.
         * @param method API method name.
         * @param pMethodParams Pointer to method parameters JSON array.
         *
         * @throws std::exception via throwParseError on validation failures.
         */
        static void parseNotification(RfTypes::RfCommand *pCommand,
                                      std::string_view method,
                                      nlohmann::json *pMethodParams);

        /**
         * @brief Helper to throw formatted parse error.
         *
         * @param commandType Type of command being parsed ("set", "get", "execute", "notify").
         * @param error Error description.
         * @param paramsJson JSON parameters as string for diagnostic output.
         *
         * @throws std::runtime_error with formatted error message.
         */
        static void throwParseError(std::string_view commandType, std::string_view error, std::string_view paramsJson);


        /**
         * @brief Extract session metadata from API request.
         *
         * @details Parses module_info object to extract logic address and RF channel.
         *
         * @param apiRequest Parsed JSON-RPC API request.
         *
         * @return SessionMetadata structure with extracted values.
         *
         * @throws std::exception if required fields are missing or invalid.
         */
        static RfTypes::SessionMetadata toMetadata(const SmartHome::API::ApiRequest &apiRequest);

        std::shared_ptr<RfClient> mpRfClient;
        std::function<void(const std::string &message)> mMessageHandler;

        /// Maximum number of parameters allowed in a single command
        static constexpr auto msMAX_PARAMETERS = 16;
        /// Maximum number of commands in a batch
        static constexpr auto msMAX_COMMANDS = msMAX_PARAMETERS;
    };
}
