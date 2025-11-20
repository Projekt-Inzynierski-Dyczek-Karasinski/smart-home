#pragma once
#include "api.h"

#include <string>
#include <string_view>

#include<nlohmann/json.hpp>

namespace SmartHome::API {
    /**
     * @brief Internal API implementation for SmartHome system communication.
     *
     * @details Handles communication between SmartHome components using
     *          command-based protocol with targets and methods.
     */
    class InternalApi final : public Api {
    public:
        /**
         * @brief Supported method types for internal commands.
         */
        enum class MethodTypes: uint8_t {
            /// Value returned on invalid stringToAction conversion
            UNKNOWN = 0,
            // Basic actions
            GET,
            SET,
            NOTIFY,
            EXECUTE,
            //Debug actions
            ECHO_REQUEST,
            PING_REQUEST
        };

        /**
         * @brief Valid targets for internal API commands.
         */
        enum class TargetTypes: uint8_t {
            /// Value returned on invalid stringToTarget conversion
            UNKNOWN = 0,
            // Interfaces
            CLI,
            GUI,
            WEB_SERVER,
            // SmartHome system components
            CORE,
            MODULE_MEDIATOR,
            DATABASE
        };

        /**
         * @brief Method type wrapper with string conversion support.
         */
        struct Method {
            MethodTypes type;

            Method() = default;


            /**
             * @brief Construct from method type.
             * @param value Method to set.
             */
            explicit Method(MethodTypes value);

            /**
             * @brief Construct from string representation.
             * @param value String to parse into method.
             */
            explicit Method(std::string_view value);

            /**
             * @brief Convert method to string representation.
             *
             * @return String view of method name.
             */
            std::string_view to_string() const;

            /**
             * @brief Parse string to method type.
             *
             * @details Case-insensitive conversion from string to MethodTypes.
             *
             * @param value String to parse.
             */
            void to_action(std::string_view value);

            /// Update method from string
            Method operator()(std::string_view value);

            /// Compare with another Method
            bool operator==(const Method &other) const;

            /// Compare with MethodTypes enum
            bool operator==(MethodTypes other) const;

        private:
            static constexpr std::string_view msGET_STRING = "get";
            static constexpr std::string_view msSET_STRING = "set";
            static constexpr std::string_view msNOTIFY_STRING = "notify";
            static constexpr std::string_view msEXECUTE_STRING = "execute";
            static constexpr std::string_view msECHO_STRING = "echo";
            static constexpr std::string_view msPING_STRING = "ping";
            static constexpr std::string_view msUNKNOWN_STRING = "unknown";
        };

        /**
         * @brief Target type wrapper with string conversion support.
         */
        struct Target {
            TargetTypes type;

            Target() = default;

            /**
             * @brief Construct from target type.
             * @param value Target to set.
             */
            explicit Target(TargetTypes value);

            /**
             * @brief Construct from string representation.
             * @param value String to parse into method.
             */
            explicit Target(std::string_view value);

            /**
             * @brief Convert target to string representation.
             *
             * @return String view of target name.
             */
            std::string_view to_string() const;

            /**
             * @brief Parse string to target type.
             *
             * @details Case-insensitive conversion from string to TargetTypes.
             *          Supports aliases (e.g., "web" for "web_server").
             *
             * @param value String to parse.
             */
            void to_target(std::string_view value);

            /// Update target from string
            Target operator()(std::string_view value);

            /// Compare with another Target
            bool operator==(const Target &other) const;

            /// Compare with TargetTypes enum
            bool operator==(TargetTypes other) const;

        private:
            static constexpr std::string_view msCLI_STRING = "cli";
            static constexpr std::string_view msGUI_STRING = "gui";
            static constexpr std::string_view msWEB_SERVER_STRING = "web_server";
            static constexpr std::string_view msWEB_STRING = "web";
            static constexpr std::string_view msCORE_STRING = "core";
            static constexpr std::string_view msMODULE_MEDIATOR_STRING = "module_mediator";
            static constexpr std::string_view msMEDIATOR_STRING = "mediator";
            static constexpr std::string_view msDATABASE_STRING = "database";
            static constexpr std::string_view msUNKNOWN_STRING = "unknown";
        };


        /**
         * @brief Parsed command structure from API request.
         */
        struct Command {
            std::optional<nlohmann::json> params; ///< Optional command parameters
            ApiId commandId;   ///< Command identifier used for result response.
            Method method;  ///< Command method to execute
            Target target;  ///< Target component for command

            /**
             * @brief Construct command from API request.
             *
             * @param value ApiRequest to parse.
             *
             * @throws std::invalid_argument If params missing or target not specified.
             *
             * @note ApiRequest must contain params with \b target key and value corresponding to one of the values
             *       in \c SmartHome::API::TargetTypes enum.\n
             *       Any additional parameters must be assigned to a list under \b method_params key.
             *       Parameters stored in \b method_params list can be of basic format or an object with key=value format.
             *       Without additional parameters \b method_params may be omitted.
             */
            explicit Command(const ApiRequest &value);

            /**
             * @brief Construct command with specific values.
             *
             * @param params Command parameters.
             * @param id Command identifier.
             * @param method Method to execute.
             * @param target Target component.
             */
            Command(const nlohmann::json &params, ApiId id, Method method, Target target);
        };

        /**
         * @brief Internal request structure containing parsed commands.
         */
        struct Request {
            /// Unique number for identifying ipc server connection, used for sending response
            connectionId_t connectionId;
            /// Vector storing parsed commands from ApiRequest structs
            std::vector<Command> commands;
            /// Flag representing if result must be sent in JSON-RPC-like structured response
            bool isResultStructured;
        };

        /**
         * @brief Internal response structure for sending results.
         *
         * @note If responses vector is empty no response will be sent.
         */
        struct Response {
            /// Unique number for identifying ipc server connection to which response will be sent
            connectionId_t connectionId;
            /// Vector of ApiResponse structs to be converted into string before sending
            std::vector<ApiResponse> apiResponses;
            /// Flag representing if result will be formated into JSON-RPC like object before conversion to string
            bool isResultStructured;
        };

        InternalApi() = default;

        ~InternalApi() override = default;

        /**
         * @brief Handle incoming API message.
         *
         * @details Parses message string (JSON-RPC or raw format), creates internal
         *          command structure, and forwards to CoreActions for processing.
         *
         * @param connectionId Connection identifier for response routing.
         * @param message Message string to process.
         */
        void handleIncoming(connectionId_t connectionId, std::string &&message) override;

        /**
         * @brief Handle outgoing API message.
         *
         * @details Posts message to IPC socket server for transmission to client.
         *
         * @param connectionId Connection identifier for response routing.
         * @param message Message string to send.
         */
        void handleOutgoing(connectionId_t connectionId, std::string &&message) override;
    };
}
