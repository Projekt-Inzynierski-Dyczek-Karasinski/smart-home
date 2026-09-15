#pragma once

#include "../api/internal_api.h"
#include "../core.h"
#include "action_helpers.h"
#include "common/time/time_provider.h"

#include <unordered_set>

#include <boost/asio.hpp>

namespace ba = boost::asio;
using sai = SmartHome::API::InternalApi;

namespace SmartHome {
    using namespace std::chrono_literals;

    /**
     * @brief Central request/response routing and command execution system.
     *
     * @details Manages the complete lifecycle of API requests and responses including:
     *          - Incoming request handling and command execution
     *          - Outgoing request aggregation and timeout management
     *          - Command handler registry and resolution
     *          - Request/response state tracking
     *          - Connection type mapping
     *
     * @note All operations are thread-safe with appropriate mutex protection.
     *       Commands are executed asynchronously using boost::asio coroutines.
     */
    class Actions {
        friend class CoreActions;
        friend class MediatorActions;
        friend class DatabaseActions;

    public:
        struct OutgoingRequestMetadata {
            std::mutex metadataMutex;
            /// Outgoing requests
            std::vector<API::ApiRequest> requestsToSend;
            /// ApiRequest response promise map {ApiRequest.id: shared_ptr<promise<ApiResponse>>}
            std::unordered_map<apiId_t, std::shared_ptr<std::promise<API::ApiResponse> > > requestsPromises;
            /// For aggregating request to send in batch
            std::shared_ptr<Time::ISteadyTimer> sendTimer;
            /// Request-level timeout timer
            std::shared_ptr<Time::ISteadyTimer> timeoutTimer;

            explicit OutgoingRequestMetadata(Time::ITimeProvider &timeProvider);
        };

        /**
         * @brief Command registry key for target-method pairs.
         */
        struct CommandKey {
            API::InternalApi::TargetTypes target; ///< Command target component
            API::InternalApi::MethodTypes action; ///< Command action type

            CommandKey() = default;

            /**
             * @brief Construct key from target and action types.
             *
             * @param newTarget Target component type.
             * @param newAction Method type.
             */
            CommandKey(API::InternalApi::TargetTypes newTarget, API::InternalApi::MethodTypes newAction);

            /**
             * @brief Extract key from command structure.
             *
             * @param command Command to extract key from.
             */
            explicit CommandKey(const API::InternalApi::Command &command);

            /// Compare with another CommandKey
            bool operator==(const CommandKey &other) const;
        };

        /**
         * @brief Hash function for CommandKey map usage.
         */
        struct CommandKeyHash {
            /// Calculate hash value for command key.
            std::size_t operator()(const CommandKey &key) const;
        };

        /**
         * @brief Command handler function type.
         *
         * @details Handlers receive command metadata and return API response asynchronously.
         */
        using CommandHandler = std::function<awaitOptApiResponse(cmdMetaPtr)>;

        using CommandsRegistry = std::unordered_map<CommandKey, CommandHandler, CommandKeyHash>;

        /// InternalApi handler type used for callbacks and forwarding outgoing requests
        using InternalApiHandler = std::function<void(connectionId_t connectionId, std::string &&response)>;


        static CommandsRegistry defaultCommandsRegistry();

        struct Config {
            std::function<bool()> isCoreRunning;
            std::shared_ptr<Utils::Logger> logger;
            ba::any_io_executor coreExecutor;
            ba::any_io_executor workerExecutor;
            ba::any_io_executor utilityExecutor;
            CommandsRegistry commandsRegistry = defaultCommandsRegistry();
            Time::ITimeProvider &timeProvider;
            InternalApiHandler handleOutgoingRequests;

            /**
             * @brief Verify that every dependency required by Actions is usable.
             *
             * @return Nothing on success, otherwise a description of detected problems.
             *
             * @note Called by \c Actions::initialize a Config that fails validation is never stored.
             */
            [[nodiscard]] std::expected<void, std::string> validate() const;
        };

        static std::expected<void, std::string> initialize(const Config &config);

        /**
         * @brief Forcefully reset \c Actions state.
         *
         * @details Clears all requests, timers and handlers without any further handling.
         *          Resets configurable variables to default state.
         */
        static void reset();

        static std::shared_ptr<const Config> getConfig();

        /**
         * @brief Process incoming API request.
         *
         * @details Creates request metadata, validates commands, starts timeout timers,
         *          and initiates asynchronous command execution.
         *
         * @param request Internal API request structure.
         * @param callback Function called when request processing completes.
         */
        static void handleIncomingRequest(const API::InternalApi::Request &request, const InternalApiHandler &callback);

        /**
         * @brief Send aggregated response for request.
         *
         * @param responseId Response identifier matching request ID.
         */
        static void handleOutgoingResponse(apiId_t responseId);

        /**
         * @brief Handles incoming response for outgoing request.
         *
         * @details Verifies if there is an awaiting request with matching connection ID and response ID.
         *          If a valid pending request exists, its response promise is fulfilled via set_value().
         *
         * @param connectionId Connection from which the response was received.
         * @param response Received response.
         *
         * @note Ignores responses without ID or without matching pending request.
         */
        static void handleIncomingResponse(connectionId_t connectionId, const API::ApiResponse &response);

        /**
         * @brief Send aggregated request.
         *
         * @details Aggregates requests by connection ID and sends them in batches.
         *          If no new messages are added within \c msAGGREGATE_OUTGOING_TIMEOUT sends the batch.
         *
         * @param connectionId Connection to which request will be sent.
         * @param apiRequest Request to send.
         * @param pResponsePromise Shared pointer to response promise.
         */
        static void handleOutgoingRequest(connectionId_t connectionId, API::ApiRequest &&apiRequest,
                                          const std::shared_ptr<std::promise<API::ApiResponse> > &pResponsePromise);


        /**
         * @brief Retrieves active request by ID.
         *
         * @param requestId ID of the request to retrieve.
         *
         * @return Request if found, std::nullopt otherwise.
         *
         * @note Thread-safe access to active requests map.
         */
        static std::optional<API::InternalApi::Request> getRequest(apiId_t requestId);

        /**
         * @brief Generate unique request ID.
         *
         * @return Next sequential ID value.
         *
         * @note Uses \c API::getNextApiId atomic counter.
         */
        static apiId_t getNextId();

        /**
         * @brief Start command timeout timer.
         *
         * @param commandMetadata Command to set timeout for.
         */
        static void startCommandTimeoutTimer(cmdMetaPtr commandMetadata);

        /**
         * @brief Cleanup handler for core shutdown.
         *
         * @details Cancels all active requests and commands, adds cancellation
         *          errors to responses, and clears request/response maps.
         */
        static void onCoreShutdown();

    private:
        /**
         * @brief Request metadata containing all commands and state.
         */
        struct RequestMetadata {
            /// Original request data
            API::InternalApi::Request request;
            /// Command metadata pointers
            std::vector<cmdMetaPtr> commands;
            /// Request-level timeout timer
            std::atomic<std::shared_ptr<Time::ISteadyTimer> > requestTimeoutTimer;
            /// Count of incomplete commands
            std::atomic<size_t> pendingCommands;
            /// Completion callback
            InternalApiHandler onComplete;

            /**
             * @brief Construct request metadata.
             *
             * @param request Original API request.
             * @param requestTimeoutTimer Timer for request timeout.
             * @param pendingCommands Initial pending command count.
             * @param onComplete Callback for request completion.
             */
            RequestMetadata(API::InternalApi::Request request,
                            std::shared_ptr<Time::ISteadyTimer> requestTimeoutTimer,
                            size_t pendingCommands,
                            InternalApiHandler onComplete);


            /**
             * @brief Cancel all commands in request.
             */
            void cancel();
        };


        /**
         * @brief Lookup command handler from registry.
         *
         * @param command Command to resolve handler for.
         * @return Handler function or nullptr if not found.
         */
        static CommandHandler resolveCommand(const API::InternalApi::Command &command,
                                             const std::shared_ptr<const Config> &cfg);


        /**
         * @brief Execute command asynchronously.
         *
         * @details Creates command metadata, validates handler, and spawns coroutine.
         *
         * @param handler Command handler function.
         * @param newCommand Command to execute.
         * @param requestId Parent request identifier.
         */
        static void executeCommandAsync(const CommandHandler &handler,
                                        const API::InternalApi::Command &newCommand,
                                        apiId_t requestId, const std::shared_ptr<const Config> &cfg);

        /**
         * @brief Coroutine for command processing.
         *
         * @param commandMetadata Command execution metadata.
         * @param handler Command handler function.
         * @return Awaitable void result.
         */
        static ba::awaitable<void> processCommand(cmdMetaPtr commandMetadata,
                                                  CommandHandler handler, std::shared_ptr<const Config> cfg);


        /**
         * @brief Process command execution result.
         *
         * @param commandMetadata Command that completed.
         * @param commandResult Result from command handler.
         */
        static void handleCommandResult(const cmdMetaPtr &commandMetadata,
                                        API::ApiResponse &&commandResult, const std::shared_ptr<const Config> &cfg);

        /**
         * @brief Handle command timeout expiration.
         *
         * @param commandMetadata Timed-out command metadata.
         */
        static void handleCommandTimeout(const cmdMetaPtr &commandMetadata, const std::shared_ptr<const Config> &cfg);

        /**
         * @brief Add command result to response collection.
         *
         * @param commandMetadata Command metadata with request ID.
         * @param apiResponse Response to add.
         */
        static void addCommandResultToResponse(const cmdMetaPtr &commandMetadata,
                                               API::ApiResponse &&apiResponse);

        /**
         * @brief Update request completion status.
         *
         * @details Decrements pending count and triggers response if complete.
         *
         * @param requestId Request to update.
         * @param cfg Active \c Actions configuration.
         */
        static void updateRequestStatus(apiId_t requestId, const std::shared_ptr<const Config> &cfg);

        /**
         * @brief Update request completion status.
         *
         * @details Decrements pending count and triggers response if complete.
         *
         * @param requestId Request to update.
         * @param cfg Active \c Actions configuration.
         *
         * @pre \c msActiveRequestsLock must be locked.
         */
        static void updateRequestStatusUnlocked(apiId_t requestId, const std::shared_ptr<const Config> &cfg);

        /**
         * @brief Handle request timeout expiration.
         *
         * @param requestId Timed-out request identifier.
         */
        static void handleRequestTimeout(apiId_t requestId, const std::shared_ptr<const Config> &cfg);

        /**
         * @brief Cleanup request resources.
         *
         * @param requestId Request to cleanup.
         */
        static void cleanupRequest(apiId_t requestId, const std::shared_ptr<const Config> &cfg);

        static std::atomic<std::shared_ptr<const Config> > mspConfig;

        /// Active request tracking map
        static std::unordered_map<apiId_t, RequestMetadata> msActiveRequests;
        /// Mutex for active requests map access
        static std::mutex msActiveRequestsLock;

        static std::unordered_map<connectionId_t, std::shared_ptr<OutgoingRequestMetadata> > msOutgoingRequests;
        static std::mutex msOutgoingRequestsLock;

        /// Response collection map
        static std::unordered_map<apiId_t, API::InternalApi::Response> msResponses;
        /// Mutex for responses map access
        static std::mutex msResponsesLock;

        static std::unordered_map<connectionId_t, sai::TargetTypes> msConnectionsMap;
        static std::shared_mutex msConnectionsMapLock;

        static std::unordered_map<sai::TargetTypes, std::unordered_set<connectionId_t> > msConnectionTypeMap;
        static std::shared_mutex msConnectionTypeMapLock;

        /// After not adding new messages to send for timeout duration, send aggregated batch message.
        static constexpr auto msAGGREGATE_OUTGOING_TIMEOUT = 10ms;
        // TODO temporary solution for timeouts when queueing get actions for few modules,
        //      caused by mediator RF communication sessions lasting ~2-5s each
        // TODO consider adding special timeout values for RF communication with modules
        static constexpr auto msREQUEST_TIMEOUT = 5 * 60 * 1000ms; ///< Request timeout timer duration in ms
        static constexpr auto msCOMMAND_TIMEOUT = 60 * 1000ms; ///< Command timeout timer duration in ms
        static constexpr auto msCLEANUP_TIMEOUT = 5000ms; ///< Timeout duration used in onCoreShutdown in ms

        // ======================================== CommandHandler functions ========================================

        /**
         * @brief Placeholder handler for unimplemented commands.
         *
         * @details Template implementation demonstrating proper async handling,
         *          timeout management, and cancellation checking.
         *
         * @param commandMetadata Command execution metadata.
         * @return API response with result or error.
         */
        static awaitOptApiResponse placeholderHandler(cmdMetaPtr commandMetadata);
    };
}
