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
        jsonParams = {
            {JsonRpcStrings::ParamsKeys::TYPE, Constants::CoreTypes::CONNECTION_TYPE},
            {JsonRpcStrings::ParamsKeys::VALUE, mTargetTypeOfClient}
        };
        request.params.emplace(jsonParams);
        request.id = API::getNextApiId();

        ba::steady_timer timer(*mpIoContext, 3s);

        // Start timeout timer
        timer.async_wait([this](const bs::error_code &ec) {
            if (!ec) {
                mpLogger->error("[SOCKET_CLIENT] Handshake timeout");
                mConnection->close();
            }
        });

        // Send handshake and await response
        std::string response;
        try {
            mConnection->write(request.to_string());
            response = mConnection->read();
        } catch (const std::exception &e) {
            mpLogger->errorf("[SOCKET_CLIENT] Error while handshaking connection: %s", e.what());
            return false;
        }
        timer.cancel();

        if (response.empty()) return false;

        API::ApiResponse apiResponse;
        try {
            mpLogger->debugf("[SOCKET_CLIENT] Handshake response: %s", response.c_str());
            apiResponse(std::string_view(response));
        } catch (const std::exception &e) {
            mpLogger->errorf("[SOCKET_CLIENT] Error while parsing handshake response: %s", e.what());
            return false;
        }

        if (apiResponse.error.has_value()) {
            mpLogger->errorf("[SOCKET_CLIENT] Handshake error: %s", apiResponse.error.value().data.c_str());
            return false;
        }

        if (apiResponse.result.has_value() && nlohmann::json::accept(apiResponse.result.value())) {
            nlohmann::json resultJson;
            try {
                resultJson = nlohmann::json::parse(apiResponse.result.value());
            } catch (const std::exception &e) {
                mpLogger->errorf("[SOCKET_CLIENT] Error while parsing handshake result: %s", e.what());
                return false;
            }

            if (resultJson.contains(JsonRpcStrings::ResponseKeys::STATUS) &&
                resultJson.at(JsonRpcStrings::ResponseKeys::STATUS).is_string() &&
                resultJson.at(JsonRpcStrings::ResponseKeys::STATUS).get<std::string_view>() == Constants::Common::OK) {
                return true;
            }
        }

        return false;
    }

    void SocketClient::startReceiving() {
        if (!mConnection || !mConnection->isOpen()) {
            mpLogger->error("[SOCKET_CLIENT] Error while listening: no connection");
            return;
        }

        ba::co_spawn(*mpIoContext, [this]() -> ba::awaitable<void> {
            while (mConnection && mConnection->isOpen()) {
                std::string message;

                try {
                    message = co_await mConnection->readAsync();
                } catch (const bs::system_error &e) {
                    mpLogger->debugf("[SOCKET_CLIENT] IPC failed while reading message: %s", e.what());
                    continue;
                } catch (const std::exception &e) {
                    mpLogger->errorf("[SOCKET_CLIENT] Unexpected error while reading message: %s", e.what());
                    continue;
                }

                if (!message.empty()) {
                    constexpr connectionId_t nullConnectionId = 0;
                    handleIncoming(nullConnectionId, std::move(message));
                }
            }
        }, ba::detached);
    }

    void SocketClient::send(const std::string_view message) {
        if (message.empty()) {
            mpLogger->error("[SOCKET_CLIENT] Error while sending: empty message");
            return;
        }
        if (mConnection && mConnection->isOpen()) {
            try {
                mConnection->writeAsync(message.data());
            } catch (const bs::system_error &e) {
                mpLogger->debugf("[SOCKET_CLIENT] IPC failed while sending: %s", e.what());
            } catch (const std::exception &e) {
                mpLogger->errorf("[SOCKET_CLIENT] Unexpected error while sending: %s", e.what());
            }
        } else {
            mpLogger->debug("[SOCKET_CLIENT] IPC failed while sending: no connection");
        }
    }
}
