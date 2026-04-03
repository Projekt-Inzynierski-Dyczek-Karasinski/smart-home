#include "socket_server_connection.h"

namespace SmartHome::IPC {
    SocketServerConnection::~SocketServerConnection() {
        SocketServerConnection::close();
    }

    void SocketServerConnection::asyncReadLoop(const std::function<void(const std::string &message)> &handleMessage) {
        auto self = shared_from_this();

        ba::co_spawn(mStrand, [this, self, handleMessage]() -> ba::awaitable<void> {
            while (isOpen()) {
                std::string message;
                try {
                    message = co_await readAsync();
                }
                catch (const bs::system_error &e) {
                    if (e.code() == ba::error::connection_reset ||
                        e.code() == ba::error::broken_pipe ) {
                        mpLogger->errorf("[SOCKET_SERVER_CONNECTION] IPC read failed, connection failed: %s", e.what());
                    }else {
                    mpLogger->debug("[SOCKET_SERVER_CONNECTION] IPC read failed, connection closed");
                    }
                    continue;
                }
                catch (const std::exception &e) {
                    mpLogger->errorf("[SOCKET_SERVER_CONNECTION] IPC read failed, unexpected error: %s", e.what());
                    continue;
                }
                if (!message.empty()) {
                    handleMessage(message);
                }
            }
        }, ba::detached);
    }

    void SocketServerConnection::close() {
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
                    mpLogger->errorf("[SOCKET_SERVER_CONNECTION] IPC connection close failed: %s",
                                     ec.message().c_str());
                }
                mpLogger->debugf("[SOCKET_SERVER_CONNECTION] IPC connection closed (ID:%d)", mConnectionId.load());
            }
        };
        std::visit(socketVisitor, mSocket);

        if (mCloseCallback) {
            const auto callback = std::move(mCloseCallback);\
            callback(mConnectionId);
        }
    }

    void SocketServerConnection::setId(const uint32_t &connectionId) {
        mConnectionId.store(connectionId);
    }

    uint32_t SocketServerConnection::getId() const {
        return mConnectionId.load();
    }

    void SocketServerConnection::setCloseCallback(std::function<void(uint32_t)> callback) {
        mCloseCallback = std::move(callback);
    }
}
