#include "api_client.h"

namespace bal = boost::asio::local;

namespace SmartHomeMediator {
    ApiClient::ApiClient(ba::io_context *io_context, const std::shared_ptr<su::Logger> &logger)
        : mpIoContext(io_context), mpLogger(logger) {
    }

    ApiClient::~ApiClient() {
        if (mConnection) {
            mConnection->close();
            mConnection.reset();
        }
    }

    bool ApiClient::connectToServer(const std::string_view udsPath) {
        mConnection.emplace(*mpIoContext, si::SocketConnection::Type::UDS, mpLogger);
        const auto endpoint = bal::stream_protocol::endpoint(udsPath);

        if (!mConnection->connect(endpoint)) {
            return false;
        }

        //TODO add id handshake
        return true;
    }

    bool ApiClient::connectToServer(const std::string_view ipAddress, const int port) {
        mConnection.emplace(*mpIoContext, si::SocketConnection::Type::UDS, mpLogger);
        const auto endpoint = bai::tcp::endpoint(bai::make_address(ipAddress), port);

        if (!mConnection->connect(endpoint)) {
            return false;
        }

        //TODO add id handshake
        return true;
    }

    void ApiClient::run(const std::function<void(const std::string &message)> &handleMessage) {
        mMessageHandler = std::move(handleMessage);
    }

    void ApiClient::startReceiving() {
        if (!mConnection || !mConnection->isOpen()) {
            mpLogger->error("[API_CLIENT] Error while listening: no connection");
            return;
        }

        auto onRead = [this](const std::string &message) {
            mMessageHandler(message);

            if (mConnection && mConnection->isOpen()) {
                ba::post(*mpIoContext, [this] {
                    startReceiving();
                });
            }
        };

        mConnection->readAsync(onRead);
    }

    void ApiClient::send(const std::string_view message) {
        if (mConnection && mConnection->isOpen()) {
            mConnection->writeAsync(message.data());
        } else {
            mpLogger->error("[API_CLIENT] Error while sending message: no connection");
        }
    }
}
