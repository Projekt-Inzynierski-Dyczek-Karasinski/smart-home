#pragma once

#include "socket_connection.h"
#include "async_logger.h"
#include "api.h"

#include <string_view>
#include <utility>

namespace sj = SmartHome::JsonRpcStrings;
namespace su = SmartHome::Utils;
namespace sa = SmartHome::API;

namespace SmartHome::IPC {
    using namespace std::chrono_literals;

    /**
     * @brief Client for communicating with the core via sockets.
     *
     * @details Implements JSON-RPC API client functionality on top of a \c SocketConnection.
     *          Provides synchronous handshake for registration and an asynchronous receive
     *          loop that dispatches incoming messages to a user-provided handler.
     */
    class SocketClient final : public API::Api {
    public:
        /**
         * @brief Construct socket client.
         *
         * @details Stores the provided IO context and logger and records the client target
         *          type used when performing the initial handshake with the core.
         *
         * @param io_context IO context for async operations.
         * @param logger Logger instance for diagnostic output.
         * @param targetTypeOfClient Logical target type used during handshake (e.g. \c "database").
         */
        explicit SocketClient(ba::io_context *io_context, const std::shared_ptr<su::Logger> &logger,
                              std::string targetTypeOfClient)
            : mpIoContext(io_context), mpLogger(logger), mTargetTypeOfClient(std::move(targetTypeOfClient)) {
        }

        /**
         * @brief Destructor, closes connection if active.
         */
        ~SocketClient() override;

        /**
         * @brief Connect to server via Unix Domain Socket.
         *
         * @details Establishes connection and performs handshake to register
         *          client with the core system.
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
         *          client with the core system.
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
         * @brief Handle outgoing socket message.
         *
         * @details Sends message to connected server.
         *
         * @param connectionId Connection identifier.
         * @param message Message string in JSON-RPC format.
         */
        void handleOutgoing(connectionId_t connectionId, std::string &&message) override;

        /**
         * @brief Handle incoming socket message.
         *
         * @details Routes incoming message to registered message handler.
         *
         * @param connectionId Connection identifier for response routing.
         * @param message Message string in JSON-RPC format.
         */
        void handleIncoming(connectionId_t connectionId, std::string &&message) override;

    private:
        /**
         * @brief Perform handshake protocol with server.
         *
         * @details Sends registration request identifying this client as a client of \c mTargetTypeOfClient.
         *          Waits up to 3 seconds for server acknowledgment.
         *
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

        static constexpr std::string_view msSET_METHOD_STRING = "set";
        static constexpr std::string_view msCORE_TARGET_STRING = "core";
        static constexpr std::string_view msCONNECTION_TYPE_STRING = "connection_type";

        ba::io_context *mpIoContext;
        std::shared_ptr<su::Logger> mpLogger;
        std::optional<SocketConnection> mConnection;

        std::function<void(const std::string &message)> mMessageHandler;

        std::string mTargetTypeOfClient{};
    };
}
