#pragma once

#include "async_logger.h"

#include <memory>
#include <variant>
#include <iostream>
#include <atomic>
#include <utility>

#include <boost/asio.hpp>
#include <boost/regex.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/copy.hpp>

namespace bio = boost::iostreams;
namespace ba = boost::asio;
namespace bai = ba::ip;
namespace bal = ba::local;
namespace bs = boost::system;

namespace SmartHome::IPC {
    // TODO consider adding reconnect functionality, with automatic reconnection on closures caused by invalid headers

    /**
    * @brief Base class for socket-based IPC connections.
    *
    * @details Provides unified interface for TCP and Unix Domain Socket connections.
    *          Supports both synchronous and asynchronous read/write operations.
    *
    * @details Uses a binary header (\c MessageHeader) for message framing — each message is preceded by a
    *          fixed-size header containing the payload length and flags
    *          (integrity check bits and per-message flags such as compression).
    *
    * @details Payloads exceeding the compression threshold are zlib-compressed automatically,
    *          the header compression flag signals this to the receiver.
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
        * @throws std::invalid_argument if socketType is invalid.
        */
        explicit SocketConnection(ba::io_context &ioContext, Type socketType,
                                  const std::shared_ptr<Utils::Logger> &logger);

        /**
        * @brief Destructor - closes connection if still open.
        */
        virtual ~SocketConnection();

        /**
         * @brief Connect to Unix Domain Socket endpoint.
         *
         * @param udsEndpoint UDS endpoint with socket path.
         *
         * @return true if connection established successfully, false on failure.
         *
         * @note Socket type must be Type::UDS. Logs debug message on failure.
         */
        bool connect(const bal::stream_protocol::endpoint &udsEndpoint);

        /**
         * @brief Connect to TCP endpoint.
         *
         * @param tcpEndpoint TCP endpoint with IP address and port.
         *
         * @return true if connection established successfully, false on failure.
         *
         * @note Socket type must be Type::TCP. Logs debug message on failure.
         */
        bool connect(const bai::tcp::endpoint &tcpEndpoint);

        /**
        * @brief Synchronously read a message from socket.
        *
        * @details Blocks until the full message is received. First reads the fixed-size \c MessageHeader to obtain
        *          payload length, then reads the payload. If the header is invalid the socket is closed
        *          and the exception is rethrown. All exceptions are propagated to the caller.
        *
        * @return Received message string.
        *
        * @throws std::runtime_error if the socket is not open.
        * @throws std::invalid_argument if the received header fails integrity check.
        * @throws boost::system::system_error on socket-level I/O errors.
        *
        * @note Thread-safety: Not safe for concurrent reads.
        */
        std::string read();

        /**
        * @brief Asynchronously read a message from socket.
        *
        * @details Reads the fixed-size \c MessageHeader first, then the payload whose length is taken from the header.
        *          Closes the socket and rethrows on invalid header. All exceptions are propagated to the caller.
        *
        * @return Awaitable resolving to the received message string.
        *
        * @throws std::runtime_error if the socket is not open.
        * @throws std::invalid_argument if the received header fails integrity check.
        * @throws boost::system::system_error on socket-level I/O errors.
        *
        * @warning Only one async read operation allowed at a time.
        */
        ba::awaitable<std::string> readAsync();

        /**
        * @brief Synchronously write a message to socket.
        *
        * @details Constructs a \c Message object (header + payload). Large payloads are compressed automatically.
        *          Sends header and payload as a single scatter-gather write.
        *          All exceptions are propagated to the caller.
        *
        * @param message Message to send.
        *
        * @throws std::runtime_error if the socket is not open.
        * @throws std::runtime_error if setting the compression flag fails.
        * @throws std::length_error if the payload exceeds uint32_t max.
        * @throws boost::system::system_error on socket-level I/O errors.
        *
        * @note Thread-safety: Not safe for concurrent writes.
        */
        void write(std::string &&message);

        /**
        * @brief Asynchronously write a message to socket.
        *
        * @details Constructs a \c Message object (header + payload). Large payloads are compressed automatically.
        *          Issues a single async scatter-gather write serialized through the internal strand.
        *          Socket-level errors are handled via \c handleError in the async callback.
        *          All other exceptions are propagated to the caller.
        *
        * @param message Message to send.
        * @param onWriteCompletion Optional callback invoked after successful write.
        *
        * @throws std::runtime_error if the socket is not open.
        * @throws std::length_error if the payload exceeds uint32_t max.
        * @throws std::runtime_error if setting the compression flag fails.
        *
        * @warning Only one async write operation allowed at a time.
        */
        void writeAsync(std::string &&message, const std::function<void()> &onWriteCompletion = nullptr);

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


        Type mType; ///< Socket type
        ba::strand<ba::io_context::executor_type> mStrand; ///< IO context strand for serialization
        std::shared_ptr<Utils::Logger> mpLogger; ///< Logger instance shared pointer

        /// Atomic flag to prevent concurrent close operations
        std::atomic<bool> mIsClosing;

    private:
        /**
         * @brief Fixed-size binary header prepended to every message.
         *
         * @details Layout:
         *              - length (uint32_t): byte length of the following payload.
         *              - flags  (std::byte): upper 3 bits hold a fixed integrity sequence (\c msINTEGRITY_SEQUENCE),
         *                lower 5 bits are per-message flags.
         *
         * @details The integrity sequence lets the receiver detect framing errors or protocol mismatches before
         *          allocating a payload buffer.
         */
        struct MessageHeader {
            uint32_t length; ///< Payload byte length
            std::byte flags; ///< Integrity bits (upper 3) + per-message flags (lower 5)

            /**
             * @brief Construct a header for the given message view.
             *
             * @param message View of the payload to be sent.
             *
             * @throws std::length_error if message length exceeds uint32_t max.
             */
            explicit MessageHeader(std::string_view message);

            MessageHeader() = default;

            /**
             * @brief Check whether the integrity sequence in \c flags is valid.
             *
             * @return true if sequence in upper bits of flag field matches \c msINTEGRITY_SEQUENCE.
             */
            [[nodiscard]] bool isValid() const;

            /**
             * @brief Set a per-message flag bit.
             *
             * @param index Bit index within the \c flags field.
             * @param value true to set the bit, false to clear it.
             *
             * @throws std::out_of_range if index is outside the valid flag range.
             */
            void setFlag(uint8_t index, bool value);

            /**
             * @brief Read a per-message flag bit.
             *
             * @param index Bit index within the \c flags field.
             *
             * @return true if the bit is set.
             *
             * @throws std::out_of_range if index is outside the valid flag range.
             */
            [[nodiscard]] bool checkFlag(uint8_t index) const;

            static constexpr uint8_t sCOMPRESSION_FLAG_INDEX = 0; ///< Flag index for zlib compression

        private:
            static constexpr std::byte msINTEGRITY_SEQUENCE{0b10100000}; ///< Expected pattern in upper 3 bits
            static constexpr std::byte msINTEGRITY_CHECK_MASK{0b11100000}; ///< Mask for integrity bits
            static constexpr std::byte msFLAGS_MASK = ~msINTEGRITY_CHECK_MASK; ///< Mask for usable flag bits

            /**
             * @brief Check whether a flag index falls within the usable bits of the \c flags field.
             *
             * @param index Bit index to validate.
             *
             * @return true if index is in range and the corresponding bit is within \c msFLAGS_MASK.
             */
            static bool isFlagIndexValid(uint8_t index);
        };
        
        /**
         * @brief Encapsulates a header-payload pair ready for transmission or reception.
         */
        struct Message {
            MessageHeader header{};
            std::string payload;

            /**
             * @brief Construct a \c Message from a string to be sent.
             *
             * @details Compresses the payload with zlib when its size exceeds \c msCOMPRESSION_THRESHOLD
             *          and sets the compression flag in the header.
             *
             * @param message Payload string.
             *
             * @throws std::length_error if the payload exceeds uint32_t max.
             * @throws std::runtime_error if setting the compression flag fails.
             */
            explicit Message(std::string &&message);

            /**
             * @brief Construct a \c Message from a received header, allocating the payload buffer.
             *
             * @details Validates the header integrity sequence and pre-allocates a payload string of the length
             *          specified in the header.
             *
             * @param messageHeader Header read from the socket.
             *
             * @throws std::invalid_argument if the header fails integrity check.
             */
            explicit Message(const MessageHeader &messageHeader);

            /**
             * @brief Extract the payload, decompressing if the compression flag is set.
             *
             * @return Final message string (moves payload out of the object).
             */
            std::string readMessage();

            /**
             * @brief Return scatter-gather buffers for a single write call.
             *
             * @return Array of two const_buffers: {header, payload}.
             */
            [[nodiscard]] std::array<ba::const_buffer, 2> toBuffers() const;

            Message() = default;

        private:
            /// 16 KB — payloads above this threshold are compressed
            static constexpr uint32_t msCOMPRESSION_THRESHOLD = 16 * 1024;

            /**
             * @brief Compress data using zlib.
             *
             * @param data Raw payload to compress.
             *
             * @return Compressed data as a string.
             */
            static std::string compress(std::string &&data);

            /**
             * @brief Decompress zlib-compressed data.
             *
             * @param data Compressed payload.
             *
             * @return Decompressed data as a string.
             */
            static std::string decompress(std::string &&data);
        };
    };
}
