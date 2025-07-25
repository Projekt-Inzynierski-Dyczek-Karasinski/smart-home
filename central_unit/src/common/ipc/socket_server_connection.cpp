#include "socket_server_connection.h"

namespace SmartHome::IPC {
    SocketServerConnection::~SocketServerConnection() {
        SocketServerConnection::close();
    }

    void SocketServerConnection::asyncReadLoop(const std::function<void(const std::string &message)> &handleMessage) {
        auto self = shared_from_this();

        readAsync([this, self, handleMessage](const std::string &message) {
            handleMessage(message);

            if (isOpen()) {
                ba::post(mIoContext, [this, self, handleMessage]() {
                    asyncReadLoop(handleMessage);
                });
            }
        });
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
                    std::cerr << "IPC connection shutdown error: " << ec.message() << std::endl;
                }
                std::cout << "IPC connection [" << mConnectionId << "] closed" << std::endl;
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
