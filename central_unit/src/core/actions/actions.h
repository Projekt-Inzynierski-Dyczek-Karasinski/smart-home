#pragma once

#include "../api/internal_api.h"
#include "../core.h"

#include <unordered_set>

#include <boost/asio.hpp>
#include <boost/algorithm/string/case_conv.hpp>

namespace ba = boost::asio;
using sai = SmartHome::API::InternalApi;

namespace SmartHome {
    using namespace std::chrono_literals;

    //TODO !pr check if needed
    class CoreActions;
    class MediatorActions;

    /**
     * @brief Actions handler for processing internal API commands.
     *
     * @details Manages command registry, request lifecycle, and asynchronous
     *          execution of API commands with timeout handling.
     */
    class Actions {
        friend class CoreActions;
        friend class MediatorActions;

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

        struct OutgoingRequestMetadata {
            std::mutex metadataMutex;
            /// Outgoing requests
            std::vector<API::ApiRequest> requestsToSend;
            /// ApiRequest response promise map {ApiRequest.id: shared_ptr<promise<ApiResponse>>}
            std::unordered_map<apiId_t, std::shared_ptr<std::promise<API::ApiResponse> > > requestsPromises;
            /// For aggregating request to send in batch
            std::shared_ptr<ba::steady_timer> sendTimer = std::make_shared<ba::steady_timer>(
                Core::Instance().getCoreIoContext());
            /// Request-level timeout timer
            std::shared_ptr<ba::steady_timer> timeoutTimer = std::make_shared<ba::steady_timer>(
                Core::Instance().getCoreIoContext());
        };

        /// Callback type for request completion notification
        using RequestCallback = std::function<void(connectionId_t connectionId, std::string &&response)>;

        /**
         * @brief Process incoming API request.
         *
         * @details Creates request metadata, validates commands, starts timeout timers,
         *          and initiates asynchronous command execution.
         *
         * @param request Internal API request structure.
         * @param callback Function called when request processing completes.
         */
        static void handleIncomingRequest(const API::InternalApi::Request &request, const RequestCallback &callback);

        /**
         * @brief Send aggregated response for request.
         *
         * @param responseId Response identifier matching request ID.
         */
        static void handleOutgoingResponse(apiId_t responseId);


        static void handleIncomingResponse(connectionId_t connectionId, const API::ApiResponse &response);

        static void handleOutgoingRequest(connectionId_t connectionId, API::ApiRequest &&apiRequest,
                                          const std::shared_ptr<std::promise<API::ApiResponse> > &pResponsePromise);


        static std::optional<API::InternalApi::Request> getRequest(apiId_t requestId);

        /**
         * @brief Start command timeout timer.
         *
         * @param commandMetadata Command to set timeout for.
         */
        static void startCommandTimeoutTimer(const std::shared_ptr<CommandMetadata> &commandMetadata);

        /**
         * @brief Cleanup handler for core shutdown.
         *
         * @details Cancels all active requests and commands, adds cancellation
         *          errors to responses, and clears request/response maps.
         */
        static void onCoreShutdown();

    private:
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


        using cmdMetaPtr = std::shared_ptr<CommandMetadata>;

        /**
         * @brief Request metadata containing all commands and state.
         */
        struct RequestMetadata {
            /// Original request data
            API::InternalApi::Request request;
            /// Command metadata pointers
            std::vector<cmdMetaPtr> commands;
            /// Request-level timeout timer
            std::atomic<std::shared_ptr<ba::steady_timer> > requestTimeoutTimer;
            /// Count of incomplete commands
            std::atomic<size_t> pendingCommands;
            /// Completion callback
            RequestCallback onComplete;

            /**
             * @brief Construct request metadata.
             *
             * @param request Original API request.
             * @param requestTimeoutTimer Timer for request timeout.
             * @param pendingCommands Initial pending command count.
             * @param onComplete Callback for request completion.
             */
            RequestMetadata(API::InternalApi::Request request,
                            std::shared_ptr<ba::steady_timer> requestTimeoutTimer,
                            size_t pendingCommands,
                            RequestCallback onComplete);


            /**
             * @brief Cancel all commands in request.
             */
            void cancel();
        };



        /**
         * @brief Generate unique request ID.
         *
         * @return Next sequential ID value.
         */
        static apiId_t getNextId();


        /**
         * @brief Command handler function type.
         *
         * @details Handlers receive command metadata and return API response asynchronously.
         */
        using CommandHandler = std::function<ba::awaitable<API::ApiResponse>(const cmdMetaPtr &)>;

        /**
         * @brief Lookup command handler from registry.
         *
         * @param command Command to resolve handler for.
         * @return Handler function or nullptr if not found.
         */
        static CommandHandler resolveCommand(const API::InternalApi::Command &command);


        /**
         * @brief Execute command asynchronously.
         *
         * @details Creates command metadata, validates handler, and spawns coroutine.
         *
         * @param handler Command handler function.
         * @param newCommand Command to execute.
         * @param requestId Parent request identifier.
         */
        static void executeCommandAsync(const CommandHandler& handler,
                                        const API::InternalApi::Command& newCommand,
                                        apiId_t requestId);

        /**
         * @brief Coroutine for command processing.
         *
         * @param commandMetadata Command execution metadata.
         * @param handler Command handler function.
         * @return Awaitable void result.
         */
        static ba::awaitable<void> processCommand(cmdMetaPtr commandMetadata,
                                                  CommandHandler handler);


        /**
         * @brief Process command execution result.
         *
         * @param commandMetadata Command that completed.
         * @param commandResult Result from command handler.
         */
        static void handleCommandResult(const cmdMetaPtr &commandMetadata,
                                        API::ApiResponse &&commandResult);

        /**
         * @brief Handle command timeout expiration.
         *
         * @param commandMetadata Timed-out command metadata.
         */
        static void handleCommandTimeout(const cmdMetaPtr &commandMetadata);

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
         * @param lockMutex Whether to lock mutex (false if already locked).
         */
        static void updateRequestStatus(apiId_t requestId, bool lockMutex = true);

        /**
         * @brief Handle request timeout expiration.
         *
         * @param requestId Timed-out request identifier.
         */
        static void handleRequestTimeout(apiId_t requestId);

        /**
         * @brief Cleanup request resources.
         *
         * @param requestId Request to cleanup.
         */
        static void cleanupRequest(apiId_t requestId);


        /// Registry mapping command keys to handler functions
        static std::unordered_map<CommandKey, CommandHandler, CommandKeyHash> msCommandsRegistry;

        /// Active request tracking map
        static std::unordered_map<apiId_t, RequestMetadata> msActiveRequests;
        /// Mutex for active requests map access
        static std::mutex msActiveRequestsLock;

        static std::unordered_map<connectionId_t, std::shared_ptr<OutgoingRequestMetadata>> msOutgoingRequests;
        static std::mutex msOutgoingRequestsLock;

        /// Response collection map
        static std::unordered_map<apiId_t, API::InternalApi::Response> msResponses;
        /// Mutex for responses map access
        static std::mutex msResponsesLock;

        static std::unordered_map<connectionId_t, sai::TargetTypes> msConnectionsMap;
        static std::mutex msConnectionsMapLock;

        static std::unordered_map<sai::TargetTypes, std::unordered_set<connectionId_t> > msConnectionTypeMap;
        static std::mutex msConnectionTypeMapLock;

        static constexpr auto ms_REQUEST_TIMEOUT = 30000ms; ///< Request timeout timer duration in ms
        static constexpr auto ms_COMMAND_TIMEOUT = 15000ms; ///< Command timeout timer duration in ms
        static constexpr auto ms_CLEANUP_TIMEOUT = 5000ms; ///< Timeout duration used in onCoreShutdown in ms

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
        static ba::awaitable<API::ApiResponse> placeholderHandler(
            const cmdMetaPtr &commandMetadata);
    };
}
