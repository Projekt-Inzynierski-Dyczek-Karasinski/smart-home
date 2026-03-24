#include "socket_connection.h"
#include "../constants/constants.h"

namespace SmartHome::IPC {
    using namespace std::string_literals;

    SocketConnection::SocketConnection(ba::io_context &ioContext,
                                       const Type socketType,
                                       const std::shared_ptr<Utils::Logger> &logger)
        : mSocket(createSocket(ioContext, socketType)),
          mType(socketType),
          mStrand(ba::make_strand(ioContext)),
          mpLogger(logger) {
    }

    SocketConnection::~SocketConnection() {
        SocketConnection::close();
    }

    bool SocketConnection::connect(const bal::stream_protocol::endpoint &udsEndpoint) {
        try {
            std::get<bal::stream_protocol::socket>(mSocket).connect(udsEndpoint);
        } catch (std::exception &e) {
            mpLogger->debugf("[SOCKET_CONNECTION] IPC UDS connection attempt failed: %s", e.what());
            return false;
        }
        return true;
    }

    bool SocketConnection::connect(const bai::tcp::endpoint &tcpEndpoint) {
        try {
            std::get<bai::tcp::socket>(mSocket).connect(tcpEndpoint);
        } catch (std::exception &e) {
            mpLogger->debugf("[SOCKET_CONNECTION] IPC TCP connection attempt failed: %s", e.what());
            return false;
        }
        return true;
    }

    std::string SocketConnection::read() {
        if (!isOpen()) throw std::runtime_error("Socket is not open");
        try {
            std::string payload;
            MessageHeader header{};
            Message messageObj;

            std::visit([this, &messageObj, &header](auto &socket) {
                ba::read(socket, ba::buffer(&header, sizeof(header)));

                try {
                    messageObj = Message(header);
                } catch (const std::exception &e) {
                    mpLogger->errorf("[SOCKET_CONNECTION] IPC read failed, invalid header: %s", e.what());
                    // Close socket on failed header read - restart needed
                    close();
                    throw;
                }

                ba::read(socket, ba::buffer(messageObj.payload));
            }, mSocket);

            return messageObj.readMessage();
        } catch (bs::system_error &e) {
            handleError(e.code());
            throw;
        } catch (const std::exception &e) {
            mpLogger->errorf("[SOCKET_CONNECTION] IPC read failed: %s", e.what());
            throw;
        }
    }

    ba::awaitable<std::string> SocketConnection::readAsync() {
        if (!isOpen()) throw std::runtime_error("Socket is not open");

        try {
            MessageHeader header{};
            // TODO rewrite on newer gcc - use visit
            if (std::holds_alternative<bai::tcp::socket>(mSocket)) {
                co_await ba::async_read(std::get<bai::tcp::socket>(mSocket),
                                        ba::buffer(&header, sizeof(header)), ba::use_awaitable);
            } else {
                co_await ba::async_read(std::get<bal::stream_protocol::socket>(mSocket),
                                        ba::buffer(&header, sizeof(header)), ba::use_awaitable);
            }

            Message messageObj;

            try {
                messageObj = Message(header);
            } catch (const std::exception &e) {
                mpLogger->errorf("[SOCKET_CONNECTION] IPC read failed, invalid header: %s", e.what());
                // Close socket on failed header read - restart needed
                close();
                throw;
            }

            // TODO rewrite on newer gcc - use visit
            if (std::holds_alternative<bai::tcp::socket>(mSocket)) {
                co_await ba::async_read(std::get<bai::tcp::socket>(mSocket),
                                        ba::buffer(messageObj.payload), ba::use_awaitable);
            } else {
                co_await ba::async_read(std::get<bal::stream_protocol::socket>(mSocket),
                                        ba::buffer(messageObj.payload), ba::use_awaitable);
            }

            co_return messageObj.readMessage();
        } catch (const bs::system_error &e) {
            handleError(e.code());
            throw;
        } catch (const std::exception &e) {
            mpLogger->errorf("[SOCKET_CONNECTION] IPC async read failed: %s", e.what());
            throw;
        }
    }

    void SocketConnection::write(std::string &&message) {
        if (!isOpen()) throw std::runtime_error("Socket is not open");

        try {
            std::visit([&message, this](auto &socket) {
                std::shared_ptr<Message> pMessageObj;

                try {
                    pMessageObj = std::make_shared<Message>(std::move(message));
                } catch (const std::exception &e) {
                    mpLogger->errorf("[SOCKET_CONNECTION] IPC write error, message parsing failed: %s", e.what());
                    throw;
                }

                ba::write(socket, pMessageObj->toBuffers());
            }, mSocket);
        } catch (const bs::system_error &e) {
            handleError(e.code());
            throw;
        } catch (const std::exception &e) {
            mpLogger->errorf("[SOCKET_CONNECTION] IPC write failed: %s", e.what());
            throw;
        }
    }


    void SocketConnection::writeAsync(std::string &&message, const std::function<void()> &onWriteCompletion) {
        if (!isOpen()) throw std::runtime_error("Socket is not open");

        std::shared_ptr<Message> pMessageObj;

        try {
            pMessageObj = std::make_shared<Message>(std::move(message));
        } catch (const std::exception &e) {
            mpLogger->errorf("[SOCKET_CONNECTION] IPC async write error, message parsing failed: %s", e.what());
            throw;
        }

        auto callback = [this, onWriteCompletion, pMessageObj](const bs::error_code &ec, const std::size_t) {
            if (!ec && isOpen()) {
                if (onWriteCompletion != nullptr) onWriteCompletion();
            } else {
                handleError(ec);
            }
        };

        auto strandWrapper = ba::bind_executor(mStrand, callback);

        std::visit([strandWrapper, pMessageObj](auto &socket) {
            ba::async_write(socket, pMessageObj->toBuffers(), strandWrapper);
        }, mSocket);
    }

    void SocketConnection::close() {
        bool expected = false;
        if (!mIsClosing.compare_exchange_strong(expected, true)) {
            return; // Is already shutting down
        }

        auto socketVisitor = [this](auto &socket) {
            if (socket.is_open()) {
                bs::error_code ec;
                socket.shutdown(boost::asio::socket_base::shutdown_both, ec);
                socket.close(ec);
                if (ec) {
                    mpLogger->debugf("[SOCKET_CONNECTION] IPC connection close failed: %s ", ec.message().c_str());
                }
                mpLogger->debug("[SOCKET_CONNECTION] IPC connection closed");
            }
        };

        std::visit(socketVisitor, mSocket);

        // TODO consider implementing on close callback
    }


    void SocketConnection::shutdownSocket(ba::socket_base::shutdown_type mode) {
        std::visit([mode](auto &socket) {
            socket.shutdown(mode);
            //TODO Sockets do not close for incoming traffic consistently - more testing needed
        }, mSocket);
    }

    bool SocketConnection::isOpen() const {
        const bool isSocketOpen = std::visit([](const auto &socket) { return socket.is_open(); }, mSocket);
        return !mIsClosing && isSocketOpen;
    }

    std::variant<bai::tcp::socket, bal::stream_protocol::socket> SocketConnection::createSocket(
        ba::io_context &ioContext, const Type socketType) {
        switch (socketType) {
            case Type::TCP:
                return bai::tcp::socket(ioContext);

            case Type::UDS:
                return bal::stream_protocol::socket(ioContext);
        }

        throw std::invalid_argument("Unknown socket type");
    }

    void SocketConnection::handleError(const bs::error_code &ec) {
        switch (ec.value()) {
            case ba::error::operation_aborted:
                mpLogger->debug("[SOCKET_CONNECTION] IPC operation aborted");
                break;
            case ba::error::eof:
                mpLogger->debug("[SOCKET_CONNECTION] IPC connection read EOF");
                close();
                break;
            case ba::error::connection_reset:
            case ba::error::broken_pipe:
                mpLogger->info("[SOCKET_CONNECTION] IPC lost connection");
                close();
                break;
            default:
                if (ec) {
                    mpLogger->debugf("[SOCKET_CONNECTION] IPC connection failed: %s ", ec.message().c_str());
                    close();
                }
        }
    }


    SocketConnection::MessageHeader::MessageHeader(const std::string_view message) : flags(msINTEGRITY_SEQUENCE) {
        if (message.length() > std::numeric_limits<decltype(length)>::max()) {
            throw std::length_error("Message too long");
        }

        length = message.length();
    }

    bool SocketConnection::MessageHeader::isValid() const {
        return (flags & msINTEGRITY_CHECK_MASK) == msINTEGRITY_SEQUENCE;
    }

    void SocketConnection::MessageHeader::setFlag(const uint8_t index, const bool value) {
        if (!isFlagIndexValid(index)) throw std::out_of_range("Invalid flag index");

        std::byte chosenFlag{1};
        chosenFlag <<= index;

        if (value) flags |= chosenFlag;
        else flags &= ~chosenFlag;
    }

    bool SocketConnection::MessageHeader::checkFlag(const uint8_t index) const {
        if (!isFlagIndexValid(index)) throw std::out_of_range("Invalid flag index");

        std::byte chosenFlag{1};
        chosenFlag <<= index;

        return static_cast<bool>(chosenFlag & flags);
    }

    bool SocketConnection::MessageHeader::isFlagIndexValid(const uint8_t index) {
        if (index >= 8) return false;

        std::byte chosenFlag{1};
        chosenFlag <<= index;

        return static_cast<bool>(chosenFlag & msFLAGS_MASK);
    }


    SocketConnection::Message::Message(std::string &&message) {
        bool shouldCompress = false;
        if (message.length() > msCOMPRESSION_THRESHOLD) {
            shouldCompress = true;
        }

        payload = shouldCompress ? compress(std::move(message)) : std::move(message);
        header = MessageHeader(payload);

        if (shouldCompress) {
            try {
                header.setFlag(MessageHeader::sCOMPRESSION_FLAG_INDEX, true);
            } catch (const std::exception &e) {
                throw std::runtime_error("Set flag failed: "s + e.what());
            }
        }
    }

    SocketConnection::Message::Message(const MessageHeader &messageHeader) {
        if (!messageHeader.isValid()) {
            throw std::invalid_argument("Invalid message header");
        }

        header = messageHeader;
        payload = std::string(header.length, '\0');
    }

    std::string SocketConnection::Message::readMessage() {
        bool isCompressed = false;
        if (header.checkFlag(MessageHeader::sCOMPRESSION_FLAG_INDEX)) {
            isCompressed = true;
        }

        return isCompressed ? decompress(std::move(payload)) : std::move(payload);
    }

    std::array<ba::const_buffer, 2> SocketConnection::Message::toBuffers() const {
        return {ba::buffer(&header, sizeof(header)), ba::buffer(payload)};
    }

    std::string SocketConnection::Message::compress(std::string &&data) {
        std::ostringstream compressed;

        // Compress in anonymous scope to finalize filtering_ostream before returning
        {
            bio::filtering_ostream out;
            out.push(bio::zlib_compressor());
            out.push(compressed);
            bio::write(out, data.data(), std::ssize(data));
        }

        return compressed.str();
    }

    std::string SocketConnection::Message::decompress(std::string &&data) {
        std::istringstream compressed(data);
        std::ostringstream decompressed;

        bio::filtering_istream in;
        in.push(bio::zlib_decompressor());
        in.push(compressed);
        bio::copy(in, decompressed);

        return decompressed.str();
    }
}
