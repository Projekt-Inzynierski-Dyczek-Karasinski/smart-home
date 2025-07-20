#pragma once

#include <atomic>
#include <memory>
#include <utility>

#include <boost/asio.hpp>

namespace ba = boost::asio;
namespace bip = boost::asio::ip;

namespace SmartHome::IPC {
    /**
     * @brief Manages TCP client connection.
     *
     * @details Manages asynchronous operations on TCP socket.
     *          Each instance represents single connection.
     *          Implements enable_shared_from_this for safe async operations.
     *
     * @note Must be created and managed via shared_ptr.
     */
    class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
    public:
        /**
         * @brief Construct TCP connection with given IO context.
         *
         * @param ioContext boost.asio IO context for async operations.
         */
        explicit TcpConnection(ba::io_context &ioContext);

        /**
         * @brief Destructor handles closing socket.
         *
         * @details Attempts graceful shutdown of socket and logs errors.
         *
         */
        ~TcpConnection();

        /**
         * @brief Get connection socket.
         *
         * @return Reference to TCP socket.
         *
         * @note Used by TcpServer during connection acceptance.
         */
        bip::tcp::socket &getSocket();

        /**
         * @brief Start asynchronous read loop.
         *
         * @details Reads data until newline character '\n'.
         *          Chains next read operation to continue reading incoming messages.
         *          Stops on receiving error code from boost asio or shutdown.
         */
        void read();

        /**
         * @brief Gracefully closes connection.
         *
         * @details Closes socket and triggers close callback.
         */
        void close();

        /**
         * @brief Set connection identifier.
         *
         * @param connectionId Unique identifier for this connection.
         */
        void setId(const uint32_t &connectionId);

        /**
         * @brief Set callback for connection close notification/cleanup.
         *
         * @param callback Function called with connection ID on close.
         */
        void setCloseCallback(std::function<void(uint32_t)> callback);

    private:
        std::atomic<bool> mIsShuttingDown{false}; ///< Flag for preventing concurrent shutdown attempts

        uint32_t mConnectionId{0}; ///< Unique id assigned by server
        std::function<void(uint32_t)> mCloseCallback; ///< Callback called on connection close

        bip::tcp::socket mSocket; ///< TCP socket for connection instance.
        ba::streambuf mStreamBuf; ///< Buffer for incoming data.
    };
}
