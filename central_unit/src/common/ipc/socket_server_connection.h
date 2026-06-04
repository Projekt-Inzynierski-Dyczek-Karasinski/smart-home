#pragma once

#include "socket_connection.h"

namespace SmartHome::IPC {
    /**
    * @brief Server-side socket connection with additional lifecycle management.
    *
    * @details Extends SocketConnection with server-specific functionality:
    *          - Connection ID management
    *          - Asynchronous read loop
    *          - Close callbacks for cleanup
    *          - Shared ownership via enable_shared_from_this
    *
    * @note Must be created and stored as shared_ptr.
    */
    class SocketServerConnection final : public SocketConnection,
                                         public std::enable_shared_from_this<SocketServerConnection> {
    public:
        using SocketConnection::SocketConnection;

        /**
        * @brief Destructor - ensures proper cleanup.
        */
        ~SocketServerConnection() override;

        /**
        * @brief Start asynchronous read loop for continuous message reception.
        *
        * @details Spawns a coroutine on the internal strand via \c ba::co_spawn.
        *          The coroutine loops with \c co_await \c readAsync(),
        *          passing each received message to \c handleMessage.
        *          Per-read exceptions are caught and logged, the loop exits only when \c isOpen() returns false.
        *
        * @param handleMessage Callback invoked for each successfully received message.
        *
        * @warning Must be called on a \c shared_ptr instance — uses \c shared_from_this internally.
        * @note Serialized through the internal strand, thread-safe with respect to concurrent writes.
        */
        void asyncReadLoop(const std::function<void(const std::string &message)> &handleMessage);

        /**
        * @brief Close connection and notify server.
        *
        * @details Invokes close callback if set before closing socket.
        *          Thread-safe, can be called multiple times.
        */
        void close() override;

        /**
        * @brief Set connection identifier.
        *
        * @param connectionId Unique connection ID assigned by server.
        */
        void setId(const uint32_t &connectionId);

        /**
        * @brief Get connection identifier.
        *
        * @return Current connection ID.
        */
        uint32_t getId() const;

        /**
        * @brief Set callback to be invoked when connection closes.
        *
        * @param callback Function called with connection ID on close.
        *
        * @note Callback is invoked exactly once, even if close() called multiple times.
        */
        void setCloseCallback(std::function<void(uint32_t)> callback);

    private:
        std::atomic<uint32_t> mConnectionId; ///< Unique connection identifier
        std::function<void(uint32_t)> mCloseCallback; ///< Callback invoked on connection close
    };
}
