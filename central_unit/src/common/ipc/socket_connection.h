#pragma once

#include "async_logger.h"

#include <memory>
#include <variant>
#include <iostream>
#include <atomic>
#include <utility>

#include <boost/asio.hpp>
#include <boost/regex.hpp>

namespace ba = boost::asio;
namespace bai = ba::ip;
namespace bal = ba::local;
namespace bs = boost::system;

namespace SmartHome::IPC {
    /**
    * @brief Base class for socket-based IPC connections.
    *
    * @details Provides unified interface for TCP and Unix Domain Socket connections.
    *          Supports both synchronous and asynchronous read/write operations.
    *          Uses CRLF (\r\n) as message delimiter for framing.
    *
    * @warning Not thread-safe for concurrent read/write operations.
    *          Use external synchronization or separate read/write connections.
    */
    class SocketConnection {
    public:
        /**
        * @brief Socket type enumeration.
        */
        enum class Type {
            TCP, ///< TCP/IP socket
            UDS ///< Unix Domain Socket
        };

        /**
        * @brief Construct a new socket connection.
        *
        * @param ioContext Boost::Asio IO context for async operations.
        * @param socketType Type of socket to create.
        * @param logger Shared pointer instance reference of logger.
        *
        * @throw std::invalid_argument if socketType is invalid.
        */
        explicit SocketConnection(ba::io_context &ioContext, Type socketType, const std::shared_ptr<Utils::Logger> &logger);

        /**
        * @brief Destructor - closes connection if still open.
        */
        virtual ~SocketConnection();

        // TODO !pr
        bool connect(const bal::stream_protocol::endpoint &udsEndpoint);

        // TODO !pr
        bool connect(const bai::tcp::endpoint &tcpEndpoint);

        /**
        * @brief Synchronously read a message from socket.
        *
        * @details Blocks until complete message (ending with \r\n) is received.
        *          Returns empty string on error or if connection is closed.
        *
        * @return Received message without delimiter, empty string on error.
        *
        * @note Thread-safety: Not safe for concurrent reads.
        */
        std::string read();

        /**
        * @brief Asynchronously read a message from socket.
        *
        * @param onReadCompletion Callback invoked with received message.
        *
        * @warning Only one async read operation allowed at a time.
        */
        void readAsync(const std::function<void(const std::string &message)> &onReadCompletion);

        /**
        * @brief Synchronously write a message to socket.
        *
        * @param message Message to send (delimiter added automatically).
        *
        * @note Thread-safety: Not safe for concurrent writes.
        */
        void write(const std::string &message);

        /**
        * @brief Asynchronously write a message to socket.
        *
        * @param message Message to send (delimiter added automatically).
        * @param onWriteCompletion Optional callback invoked after successful write.
        *
        * @warning Only one async write operation allowed at a time.
        */
        void writeAsync(const std::string &message, const std::function<void()> &onWriteCompletion = nullptr);

        /**
        * @brief Close the connection gracefully.
        *
        * @details Thread-safe, can be called multiple times.
        *          Performs shutdown before close to ensure data is flushed.
        */
        virtual void close();

        //TODO Sockets do not close for incoming traffic consistently - more testing needed
        /**
         * @brief Shutdowns connection socket according to passed mode.
         *
         * @param mode boost.asio socket shutdown mode.
         */
        void shutdownSocket(ba::socket_base::shutdown_type mode);

        /**
        * @brief Check if socket is open.
        *
        * @return true if socket is open and not closing, false otherwise.
        */
        bool isOpen() const;

        // TODO consider making an getter for socket
        std::variant<bai::tcp::socket, bal::stream_protocol::socket> mSocket;

    protected:
        /// Message delimiter
        static constexpr std::string_view ms_MESSAGE_DELIMITER = "\r\n";
        /// Compiled regex for delimiter matching
        static const boost::regex ms_DELIMITER_REGEX;

        /**
        * @brief Factory method to create appropriate socket type.
        */
        static std::variant<bai::tcp::socket, bal::stream_protocol::socket> createSocket(
            ba::io_context &ioContext, Type socketType);

        /**
        * @brief Handle socket errors.
        *
        * @param ec Error code from Boost::Asio operation.
        *
        * @note May close connection on fatal errors.
        */
        void handleError(const bs::error_code &ec);

        /**
        * @brief Extract message from stream buffer.
        *
        * @param bytesTransferred Number of bytes read including delimiter.
        * @return Message without delimiter.
        */
        std::string getMessageFromBuffer(const size_t &bytesTransferred);

        Type mType; ///< Socket type
        ba::io_context::strand mStrand; ///< IO context strand for serialization
        std::shared_ptr<Utils::Logger> mpLogger; ///< Logger instance shared pointer
        ba::streambuf mStreamBuf; ///< Buffer for async read operations

        /// Atomic flag to prevent concurrent close operations
        std::atomic<bool> mIsClosing;
    };
}
