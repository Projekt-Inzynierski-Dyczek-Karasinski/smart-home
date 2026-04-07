#pragma once
#include "api.h"
#include "constants.h"
#include "../api/internal_api.h"

#include <expected>

#include <boost/asio.hpp>

namespace SmartHome {
    namespace ba = boost::asio;

    namespace jp = JsonRpcStrings::ParamsKeys;
    namespace jr = JsonRpcStrings::ResponseKeys;
    namespace cc = Constants::Common;
    namespace co = Constants::OperationModes;
    namespace cct = Constants::CoreTypes;
    namespace cmt = Constants::MediatorTypes;
    namespace cdi = Constants::DatabaseIdentifiers;
    namespace cdck = Constants::DeviceConfigKeys;
    namespace cmck = Constants::ModuleConfigKeys;


    using awaitOptApiResponse = ba::awaitable<std::optional<API::ApiResponse> >;

    using jsonPtr = std::shared_ptr<nlohmann::json>;

    template<typename T>
    using ValidationResult = std::expected<T, API::ApiError>;

    /**
     * @brief Shared utilities and command metadata for action handlers.
     *
     * @details Provides:
     *          - \c CommandMetadata structure tracking the lifecycle of a single command execution
     *          - Parameter validation helpers (require.../resolve...) used across all action handler classes
     *          - Database query helper for dispatching GET/SET requests to the db-service
     *          - Mediator delegation helper for forwarding commands to modules
     *
     * @note All validation helpers return \c ValidationResult which is \code{cpp} std::expected<T, ApiError>\endcode.
     *       On failure they carry a ready-to-send \c ApiError — callers should propagate it directly.
     */
    class ActionHelpers {
    public:
        /**
         * @brief Command execution metadata and state tracking.
         */
        struct CommandMetadata {
            /**
             * @brief Command execution states.
             */
            enum class State : uint8_t {
                PENDING = 0,
                COMPLETED = 1,
                CANCELLED = 2,
                TIMED_OUT = 3
            };


            /// Command data
            API::InternalApi::Command command;
            /// Command-specific timeout timer
            std::atomic<std::shared_ptr<ba::steady_timer> > commandTimeoutTimer;
            /// Parent request ID
            apiId_t requestId;
            /// Current command state
            std::atomic<State> state{State::PENDING};
            /// Helper flag signaling notification command
            bool isNotification = false;

            /**
             * @brief Construct command metadata.
             *
             * @param command Command to execute.
             * @param commandTimeoutTimer Timer for command timeout.
             * @param requestId Parent request identifier.
             */
            CommandMetadata(API::InternalApi::Command command,
                            std::shared_ptr<ba::steady_timer> commandTimeoutTimer,
                            apiId_t requestId);

            /**
             * @brief Cancel command execution.
             *
             * @return true if successfully cancelled, false if already completed.
             */
            bool cancel();

            /**
             * @brief Check if command is still pending.
             *
             * @return true if command state is PENDING, false otherwise.
             */
            bool isPending() const;
        };

        using cmdMetaPtr = std::shared_ptr<CommandMetadata>;

        /**
         * @brief Require params object in command metadata.
         *
         * @param cmdMetadata Command metadata containing the command.
         *
         * @return Shared pointer to params JSON object on success, API error if params are missing or not an object.
         */
        static ValidationResult<jsonPtr> requireParams(const CommandMetadata &cmdMetadata);

        /**
         * @brief Require type field in params.
         *
         * @param params Request params JSON.
         *
         * @return Type string on success, API error on validation failure.
         */
        static ValidationResult<std::string> requireType(const nlohmann::json &params);

        /**
         * @brief Require module_id in params.
         *
         * @param params Request params JSON.
         *
         * @return Module id on success, API error on validation failure.
         */
        static ValidationResult<uint> requireModuleId(const nlohmann::json &params);

        /**
         * @brief Require limit argument in params.
         *
         * @param params Request params JSON.
         *
         * @return Limit value on success, API error on validation failure.
         */
        static ValidationResult<uint> requireLimitArg(const nlohmann::json &params);

        /**
         * @brief Require device logic id argument in params.
         *
         * @param params Request params JSON.
         *
         * @return Device logic id on success, API error on validation failure.
         */
        static ValidationResult<uint> requireDeviceLogicIdArg(const nlohmann::json &params);


        /**
         * @brief Resolve device id from params.
         *
         * @details Resolves from either device_id or module_id + device_logic_id.
         *
         * @param params Request params JSON.
         *
         * @return Resolved device id on success, API error on failure.
         */
        static ValidationResult<uint> resolveDeviceId(const nlohmann::json &params);

        /**
         * @brief Require mode field in params.
         *
         * @details Validates that the mode value is one of the allowed values defined in \c Constants::OperationModes.
         *
         * @param params Request params JSON.
         *
         * @return Mode string on success, API error on validation failure.
         */
        static ValidationResult<std::string> requireMode(const nlohmann::json &params);

        /**
         * @brief Require path field in params.
         *
         * @details Validates that the path is a non-empty dot-separated string, that the first key is a valid
         *          database identifier, and that the second key (if present) is a valid config key.
         *
         * @param params Request params JSON.
         *
         * @return Path string on success, API error on validation failure.
         */
        static ValidationResult<std::string> requirePath(const nlohmann::json &params);

        /**
         * @brief Require value field in params.
         *
         * @param params Request params JSON.
         *
         * @return Value JSON on success, API error if value field is missing.
         */
        static ValidationResult<nlohmann::json> requireValue(const nlohmann::json &params);

        /**
         * @brief Execute database query and validate the response.
         *
         * @details Sends a request to the db-service and validates the response structure.
         *          For GET method, validates presence of \c rows array.
         *          For SET method, validates presence of \c affected_rows field.
         *
         * @param method Database method name (e.g. \c Constants::Methods::GET or \c Constants::Methods::SET).
         * @param dbQuery JSON query payload for db-service.
         *
         * @return Parsed JSON response on success, API error on failure.
         */
        static ba::awaitable<ValidationResult<nlohmann::json> > queryDatabase(std::string_view method,
                                                                              nlohmann::json &&dbQuery);

        /**
         * @brief Delegate command to mediator for resolution.
         *
         * @details Forwards the command to \c MediatorActions::mediatorSetHandler for commands
         *          targeted at a module or mediator.
         *
         * @param pCommandMetadata Command execution metadata.
         * @param params Request params JSON (unused, resolution handled by mediator).
         *
         * @return API response from mediator handler.
         */
        static awaitOptApiResponse delegateToMediator(cmdMetaPtr pCommandMetadata, jsonPtr params);
    };

    // Share cmdMetaPtr in SmartHome namespace
    using cmdMetaPtr = ActionHelpers::cmdMetaPtr;
}
