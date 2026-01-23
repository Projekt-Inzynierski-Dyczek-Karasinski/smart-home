#pragma once

#include "socket_connection.h"
#include "async_logger.h"
#include "api.h"

#include <string_view>


namespace si = SmartHome::IPC;
namespace su = SmartHome::Utils;
namespace sa = SmartHome::API;

namespace SmartHomeMediator {
    /**
     * @brief Client for JSON-RPC API communication with SmartHome server.
     *
     * @details Manages connection lifecycle, handshake protocol, and bidirectional message routing.
     *          Supports both Unix Domain Socket (UDS) and TCP connections.
     *          Implements sa::Api interface for message handling.
     */
    class ApiClient final : public sa::Api {
    public:
        /**
         * @brief Construct API client.
         *
         * @param io_context IO context for async operations.
         * @param logger Logger instance for diagnostic output.
         */
        explicit ApiClient(ba::io_context *io_context, const std::shared_ptr<su::Logger> &logger)
            : mpIoContext(io_context), mpLogger(logger) {
        }

        /**
         * @brief Destructor, closes connection if active.
         */
        ~ApiClient() override;

        /**
         * @brief Connect to server via Unix Domain Socket.
         *
         * @details Establishes connection and performs handshake to register
         *          as mediator module with the core system.
         *
         * @param udsPath Path to Unix Domain Socket.
         *
         * @return true on successful connection and handshake, false otherwise.
         */
        bool connectToServer(std::string_view udsPath);

        /**
         * @brief Connect to server via TCP.
         *
         * @details Establishes connection and performs handshake to register
         *          as mediator module with the core system.
         *
         * @param ipAddress IP address of the server.
         * @param port TCP port number.
         *
         * @return true on successful connection and handshake, false otherwise.
         */
        bool connectToServer(std::string_view ipAddress, int port);


        /**
         * @brief Initialize message handling and start receiving messages.
         *
         * @param messageHandler Callback function invoked for each received message.
         *
         * @note Must be called after successful connection before messages can be received.
         */
        void initialize(const std::function<void(const std::string &message)> &messageHandler);


        /**
         * @brief Handle outgoing API message.
         *
         * @details Sends message to connected server.
         *
         * @param connectionId Connection identifier.
         * @param message Message string in JSON-RPC format.
         */
        void handleOutgoing(SmartHome::connectionId_t connectionId, std::string &&message) override;

        /**
         * @brief Handle incoming API request.
         *
         * @details Routes incoming message to registered message handler.
         *
         * @param connectionId Connection identifier for response routing.
         * @param message Message string in JSON-RPC format.
         */
        void handleIncoming(SmartHome::connectionId_t connectionId, std::string &&message) override;

    private:
        // TODO consider moving handshake to sa::API

        /**
         * @brief Perform handshake protocol with server.
         *
         * @details Sends registration request identifying this client as a mediator module.
         *          Waits up to 3 seconds for server acknowledgment.
         *
         * @return true if handshake succeeds, false on timeout or error.
         */
        bool handshake();

        /**
         * @brief Start asynchronous message receiving loop.
         *
         * @details Continuously reads messages from connection and invokes message handler.
         *          Re-posts itself to continue receiving until connection closes.
         */
        void startReceiving();

        /**
         * @brief Send message to server.
         *
         * @param message Message string to send.
         *
         * @note Logs error if connection is not open or message is empty.
 */
        void send(std::string_view message);

        ba::io_context *mpIoContext;
        std::shared_ptr<su::Logger> mpLogger;
        std::optional<si::SocketConnection> mConnection;

        std::function<void(const std::string &message)> mMessageHandler;

        static constexpr std::string_view msSET_METHOD_STRING = "set";
        static constexpr std::string_view msCORE_TARGET_STRING = "core";
        static constexpr std::string_view msMEDIATOR_TARGET_STRING = "module_mediator";
        static constexpr std::string_view msCONNECTION_TYPE_STRING = "connection_type";

    };
}
