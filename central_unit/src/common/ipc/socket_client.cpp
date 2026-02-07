#include "socket_client.h"
#include "../constants/constants.h"

namespace SmartHome::IPC {
    SocketClient::~SocketClient() {
        if (mConnection) {
            mConnection->close();
            mConnection.reset();
        }
    }

    bool SocketClient::connectToServer(const std::string_view udsPath) {
        mConnection.emplace(*mpIoContext, SocketConnection::Type::UDS, mpLogger);
        const auto endpoint = bal::stream_protocol::endpoint(udsPath);

        if (!mConnection->connect(endpoint)) {
            return false;
        }

        return handshake();
    }

    bool SocketClient::connectToServer(const std::string_view ipAddress, const int port) {
        mConnection.emplace(*mpIoContext, SocketConnection::Type::TCP, mpLogger);
        const auto endpoint = bai::tcp::endpoint(bai::make_address(ipAddress), port);

        if (!mConnection->connect(endpoint)) {
            return false;
        }

        return handshake();
    }

    void SocketClient::initialize(const std::function<void(const std::string &message)> &messageHandler) {
        mMessageHandler = messageHandler;

        startReceiving();
    }

    void SocketClient::handleOutgoing(connectionId_t connectionId, std::string &&message) {
        send(message);
    }

    void SocketClient::handleIncoming(connectionId_t connectionId, std::string &&message) {
        mMessageHandler(message);
    }

    bool SocketClient::handshake() {
        // Build handshake request
        API::ApiRequest request;
        nlohmann::json jsonParams;
        request.method = API::getTargetMethodString(Constants::Targets::CORE, Constants::Methods::SET);
        jsonParams[Constants::CoreTypes::CONNECTION_TYPE] = mTargetTypeOfClient;
        request.params.emplace(jsonParams);
        request.id = API::getNextApiId();

        ba::steady_timer timer(*mpIoContext, 3s);

        // Start timeout timer
        timer.async_wait([this](const bs::error_code &ec) {
            if (!ec) {
                mpLogger->error("[API_CLIENT] Handshake timeout");
                mConnection->close();
            }
        });

        // Send handshake and await response
        std::string response;
        try {
            mConnection->write(request.to_string());
            response = mConnection->read();
        } catch (const std::exception &e) {
            mpLogger->errorf("[API_CLIENT] Error while handshaking connection: %s", e.what());
            return false;
        }
        timer.cancel();

        if (response.empty()) return false;

        API::ApiResponse apiResponse;
        try {
            mpLogger->debugf("[API_CLIENT] Handshake response: %s", response.c_str());
            apiResponse(std::string_view(response));
        } catch (const std::exception &e) {
            mpLogger->errorf("[API_CLIENT] Error while parsing handshake response: %s", e.what());
            return false;
        }

        if (apiResponse.error.has_value()) {
            mpLogger->errorf("[API_CLIENT] Handshake error: %s", apiResponse.error.value().data.c_str());
            return false;
        }

        if (apiResponse.result.has_value() &&
            apiResponse.result.value() == jsonParams.dump()) {
            return true;
        }

        return false;
    }

    void SocketClient::startReceiving() {
        if (!mConnection || !mConnection->isOpen()) {
            mpLogger->error("[API_CLIENT] Error while listening: no connection");
            return;
        }

        auto onRead = [this](const std::string &message) {
            constexpr connectionId_t nullConnectionId = 0;
            handleIncoming(nullConnectionId, message.data());

            if (mConnection && mConnection->isOpen()) {
                ba::post(*mpIoContext, [this] {
                    startReceiving();
                });
            }
        };

        mConnection->readAsync(onRead);
    }

    void SocketClient::send(const std::string_view message) {
        if (message.empty()) {
            mpLogger->error("[API_CLIENT] Error while sending: empty message");
            return;
        }
        if (mConnection && mConnection->isOpen()) {
            mConnection->writeAsync(message.data());
        } else {
            mpLogger->error("[API_CLIENT] Error while sending message: no connection");
        }
    }
}
