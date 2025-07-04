#pragma once

#include <atomic>
#include <memory>
#include <thread>
#include <utility>

#include <boost/asio.hpp>

namespace ba = boost::asio;
namespace bip = boost::asio::ip;

namespace SmartHome::IPC {
    /**
     * @brief Class handling TCP server connections
     *
     * @details Manages asynchronous operations on TCP socket.
     *          Single instance represents single connection.
     *
     * @note Must be created and managed via shared_ptr
     */
    class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
    public:
        /**
         * @brief Construct TCP connection with given IO context
         *
         * @param ioContext boost.asio IO context for async operations
         */
        TcpConnection(ba::io_context &ioContext);

        /**
         * @brief Destructor handles closing socket
         *
         */
        ~TcpConnection();

        /**
         * @brief Get connection socket
         *
         * @return Reference to TCP socket
         *
         * @note Used by TcpServer during connection acceptance
         */
        bip::tcp::socket &getSocket();

        /**
         * @brief Start asynchronous read operation
         *
         * @details Reads data until newline character '\n'.
         *          Chains next read operation to continue reading incoming messages.
         *          Stops on receiving error code from boost asio.
         */
        void read();

    private:
        bip::tcp::socket mSocket; ///< TCP socket for connection instance
        ba::streambuf mStreamBuf; ///< Buffer for incoming data
    };
}