#pragma once
#include "rf_driver/hc12_driver.h"
#include "rf_types.h"

#include <string>
#include <chrono>
#include <vector>
#include <boost/asio.hpp>


namespace ba = boost::asio;

namespace SmartHomeMediator {
    class RfClient;
    using namespace std::chrono_literals;

    /**
     * @brief RF communication session handler.
     *
     * @details Manages bidirectional RF communication with a single module.
     *          Handles packet assembly/disassembly, retries, and state machine for request-response or notification flows.
     *          Supports both central-initiated and module-initiated sessions.
     */
    class Session {
    public:
        /**
         * @brief Construct RF communication session.
         *
         * @param metadata Session configuration and command queue.
         * @param pRfDriver RF driver for hardware communication.
         * @param pRfClient Parent RF client for callback access.
         * @param pLogger Logger instance.
         */
        Session(RfTypes::SessionMetadata &&metadata,
            const std::shared_ptr<HC12Driver> &pRfDriver,
            const std::shared_ptr<RfClient> &pRfClient,
            const std::shared_ptr<su::Logger> &pLogger);

        /**
         * @brief Execute session state machine.
         *
         * @details Runs complete session lifecycle: connection establishment, command execution,
         *          response handling, and cleanup. Implements timeout and retry logic.
         *
         * @return Awaitable with JSON-RPC formatted result string, or empty on notification.
         */
        ba::awaitable<std::string> execute();

        /**
         * @brief Add received packet to session buffer.
         *
         * @details Thread-safe packet queuing for async receive loop.
         *          Sets ready flag when complete message assembled.
         *
         * @param packet Validated RF packet.
         */
        void addReceivedPacket(RfTypes::Packet packet);

    private:
        /**
         * @brief Session state machine states.
         *
         * @details Defines all possible states in session execution flow.
         *          Flow depends on session type (central-initiated vs module-initiated).
         */
        enum class State : uint8_t {
            NEXT_COMMAND, ///< Load next command from queue
            SEND_END_COMMAND, ///< Transmit END marker to close session
            SEND_ACK_COMMAND, ///< Send ACK (acknowledgment)
            SEND_NEG_COMMAND,  ///< Send NEG (negative acknowledgment)
            SEND_MESSAGE, ///< Transmit current command message
            SEND_REPEAT_LAST_MESSAGE, ///< Request module to repeat last message
            RESEND_LAST_MESSAGE, ///< Retransmit last sent message
            AWAIT_RESPONSE, ///< Wait for module response to request
            AWAIT_NOTIFICATION, ///< Wait for module-initiated notification
            FINISHED ///< Session complete, ready to return result
        };

        /**
         * @brief Session execution state and context.
         *
         * @details Encapsulates complete state machine context for single session execution.
         *          Tracks current/previous states, command queue position, retry count, message buffers,
         *          and API response accumulation. Provides state transition helper with history tracking.
         */
        struct ExecutionContext {
            State currentState;
            State previousState;

            /// Response command received from module (for request-response flow)
            std::unique_ptr<RfTypes::RfCommand> pCommandResponse;
            /// Current high-level command being executed
            std::unique_ptr<RfTypes::RfCommand>  pCurrentCommand;
            /// Low-level protocol command (ACK, NEG, REPEAT, END)
            RfTypes::RfCommand lowLevelCommand;

            std::vector<uint8_t> receivedMessage;
            std::vector<uint8_t> lastSendMessage;

            /// Accumulated JSON-RPC result strings from executed commands
            std::vector<std::string> resultsVector;

            SmartHome::API::ApiResponse response;
            SmartHome::API::ApiError error;

            /// Current position in \c mMetadata.commands vector
            size_t commandsVectorIndex = 0;
            uint retries = 0;
            /// true if session started by module notification, false if central-initiated
            bool isInitializedFromModule;

            /// Pointer to shared steady_timer for delays between operations
            ba::steady_timer* delayTimer;

            /**
             * @brief State transition function with history tracking.
             *
             * @details Lambda that updates previousState and state atomically.
             *          Provides clean state change interface for processState().
             */
            std::function<void(State)> changeState;

            /**
             * @brief Construct execution context with initial state.
             *
             * @param initFromModule true if session initiated by module (notification),
             *                       false if initiated by central unit (request).
             * @param timer Pointer to shared timer for operation delays.
             *
             * @details Sets initial state to AWAIT_NOTIFICATION for module-initiated sessions or
             *          NEXT_COMMAND for central-initiated sessions.
             *          Initializes changeState lambda for state transitions.
             */
            ExecutionContext(bool initFromModule, ba::steady_timer* timer);

        };

        /**
         * @brief Handle mediator configuration command session.
         *
         * @return Awaitable with JSON-RPC response string.
         */
        ba::awaitable<std::string> handleMediatorConfigSession() const;

        /**
         * @brief Process current state in session state machine.
         *
         * @param ctx Execution context with current state and data.
         */
        ba::awaitable<void> processState(ExecutionContext& ctx);

        /**
         * @brief Send message via RF driver with proper delays.
         *
         * @param message Binary message to transmit.
         */
        ba::awaitable<void> send(const std::vector<uint8_t> &message) const;

        /**
         * @brief Receive message from packet buffer.
         *
         * @details Polls mReceivedBuffer until complete message ready or timeout.
         *
         * @return Awaitable with assembled message bytes.
         */
        ba::awaitable<std::vector<uint8_t> > receive();

        /**
         * @brief Change RF channel if driver supports multi-channel.
         *
         * @param channel Target RF channel number.
         *
         * @return Awaitable with true on success, false on failure.
         */
        ba::awaitable<bool> changeChannel(uint8_t channel) const;

        /**
         * @brief Acquire exclusive connection to target module.
         *
         * @details Sends initial handshake and waits for acknowledgment.
         *
         * @return Awaitable with true if connection established, false on failure.
         */
        ba::awaitable<bool> acquireConnection();

        /// Maximum number of retry attempts per command
        static constexpr uint msMAX_REATTEMPTS = 3;
        /// Total session timeout including all commands
        static constexpr auto msSESSION_TIMEOUT = 10s;
        /// Timeout for receiving single message response
        static constexpr auto msRECEIVE_MESSAGE_TIMEOUT = 2s;
        /// Delay between buffer polling attempts
        static constexpr auto msPOOLING_DELAY = 10ms;

        RfTypes::SessionMetadata mMetadata;
        std::shared_ptr<RfDriver> mpRfDriver;
        std::weak_ptr<RfClient> mpRfClient;
        std::shared_ptr<su::Logger> mpLogger;

        std::vector<uint8_t> mReceivedBuffer;
        std::mutex mReceiveMutex;
        std::atomic_bool mIsReceivedBufferReady{false};

    };
}
