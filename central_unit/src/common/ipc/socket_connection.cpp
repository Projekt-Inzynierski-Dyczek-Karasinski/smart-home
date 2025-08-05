#include "socket_connection.h"

namespace SmartHome::IPC {
    SocketConnection::SocketConnection(ba::io_context &ioContext, const Type socketType, const std::shared_ptr<Utils::Logger> &logger)
        : mSocket(createSocket(ioContext, socketType)), mType(socketType), mIoContext(ioContext), mpLogger(logger) {
    };

    SocketConnection::~SocketConnection() {
        SocketConnection::close();
    };

    const boost::regex SocketConnection::ms_DELIMITER_REGEX(SocketConnection::ms_MESSAGE_DELIMITER);

    std::string SocketConnection::read() {
        if (isOpen()) {
            size_t bytesTransferred = 0;
            try {
                std::visit([this, &bytesTransferred ](auto &socket) {
                    bytesTransferred = ba::read_until(socket, mStreamBuf, ms_DELIMITER_REGEX);
                }, mSocket);
                return getMessageFromBuffer(bytesTransferred); // Returns message payload w/o delimiter
            } catch (bs::system_error &e) {
                handleError(e.code());
            }
        }
        return ""; //Empty string on read failure
    }

    void SocketConnection::readAsync(const std::function<void(const std::string &)> &onReadCompletion) {
        if (!isOpen()) return;

        auto callback = [this, onReadCompletion](const bs::error_code &ec, const std::size_t bytesTransferred) {
            if (!ec && isOpen()) {
                onReadCompletion(getMessageFromBuffer(bytesTransferred));
            } else {
                handleError(ec);
            }
        };

        std::visit([this, callback](auto &socket) {
            ba::async_read_until(socket, mStreamBuf, ms_DELIMITER_REGEX, callback);
        }, mSocket);
    }


    void SocketConnection::write(const std::string &message) {
        if (!isOpen()) return;

        try {
            std::visit([this, message](auto &socket) {
                ba::write(socket, ba::buffer(message + ms_MESSAGE_DELIMITER));
            }, mSocket);
        } catch (bs::system_error &e) {
            handleError(e.code());
        }
    }

    void SocketConnection::writeAsync(const std::string &message, const std::function<void()> &onWriteCompletion) {
        if (!isOpen()) return;

        auto callback = [this, onWriteCompletion](const bs::error_code &ec, const std::size_t bytesTransferred) {
            if (!ec && isOpen()) {
                if (onWriteCompletion != nullptr) onWriteCompletion();
            } else {
                handleError(ec);
            }
        };

        std::visit([this, message, callback](auto &socket) {
            ba::async_write(socket, ba::buffer(message + ms_MESSAGE_DELIMITER), callback);
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
                socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
                socket.close(ec);
                if (ec) {
                    mpLogger->errorf("[SOCKET_CONNECTION] IPC connection close failed: %s ", ec.message().c_str());
                }
                mpLogger->debug("[SOCKET_CONNECTION] IPC connection closed");
            }
        };

        std::visit(socketVisitor, mSocket);
    }

    bool SocketConnection::isOpen() const {
        const bool isSocketOpen = std::visit([](const auto &socket) { return socket.is_open(); }, mSocket);
        return (!mIsClosing && isSocketOpen);
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
                mpLogger->info("[SOCKET_CONNECTION] IPC operation aborted");
                break;
            case ba::error::eof:
                mpLogger->info("[SOCKET_CONNECTION] IPC connection read EOF");
                close();
                break;
            case ba::error::connection_reset:
            case ba::error::broken_pipe:
                mpLogger->info("[SOCKET_CONNECTION] IPC lost connection");
                close();
                break;
            default:
                if (ec) {
                    mpLogger->errorf("[SOCKET_CONNECTION] IPC connection failed: %s ", ec.message().c_str());
                    close();
                }
        }
    }

    std::string SocketConnection::getMessageFromBuffer(const size_t &bytesTransferred) {
        std::istream is(&mStreamBuf);
        std::string message(bytesTransferred, '\0');
        is.read(&message[0], static_cast<long>(bytesTransferred));

        // Cut delimiter from message
        if (message.size() > strlen(ms_MESSAGE_DELIMITER) && message.ends_with(ms_MESSAGE_DELIMITER)) {
            message.resize(message.size() - strlen(ms_MESSAGE_DELIMITER));
        }

        return message;
    }
}
