#include "api_client.h"

namespace bal = boost::asio::local;

namespace SmartHomeMediator {
    using namespace std::chrono_literals;

    namespace sj = SmartHome::JsonRpcStrings;

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

        return handshake();
    }

    bool ApiClient::connectToServer(const std::string_view ipAddress, const int port) {
        mConnection.emplace(*mpIoContext, si::SocketConnection::Type::TCP, mpLogger);
        const auto endpoint = bai::tcp::endpoint(bai::make_address(ipAddress), port);

        if (!mConnection->connect(endpoint)) {
            return false;
        }

        return handshake();
    }

    void ApiClient::initialize(const std::function<void(const std::string &message)> &messageHandler) {
        mMessageHandler = messageHandler;

        startReceiving();
    }

    void ApiClient::handleOutgoing(SmartHome::connectionId_t connectionId, std::string &&message) {
        send(message);
    }

    void ApiClient::handleIncoming(SmartHome::connectionId_t connectionId, std::string &&message) {
        mMessageHandler(message);
    }

    bool ApiClient::handshake() {
        // Build handshake request
        SmartHome::API::ApiRequest request;
        nlohmann::json jsonParams;
        request.method = msSET_METHOD_STRING;
        jsonParams[sj::ParamsKeys::TARGET] = msCORE_TARGET_STRING;
        jsonParams[sj::ParamsKeys::METHOD_PARAMS] = {msCONNECTION_TYPE_STRING, msMEDIATOR_TARGET_STRING};
        request.params.emplace(jsonParams);
        request.id = 1;

        ba::steady_timer timer(*mpIoContext, 3s);

        // Start timeout timer
        timer.async_wait([this](const bs::error_code &ec){
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
        }
        catch (const std::exception &e) {
            mpLogger->errorf("[API_CLIENT] Error while handshaking connection: %s", e.what());
            return false;
        }
        timer.cancel();

        if (response.empty()) return false;

        SmartHome::API::ApiResponse apiResponse;
        try {
            mpLogger->debugf("[API_CLIENT] Handshake response: %s", response.c_str());
            apiResponse(std::string_view(response));
        }
        catch (const std::exception &e) {
            mpLogger->errorf("[API_CLIENT] Error while parsing handshake response: %s", e.what());
            return false;
        }

        if (apiResponse.error.has_value()) {
            mpLogger->errorf("[API_CLIENT] Handshake error: %s", apiResponse.error.value().data.c_str());
            return false;
        }

        if (apiResponse.result.has_value() && apiResponse.result.value() == jsonParams[sj::ParamsKeys::METHOD_PARAMS].dump()) {
            return true;
        }

        return false;
    }

    void ApiClient::startReceiving() {
        if (!mConnection || !mConnection->isOpen()) {
            mpLogger->error("[API_CLIENT] Error while listening: no connection");
            return;
        }

        auto onRead = [this](const std::string &message) {
            constexpr SmartHome::connectionId_t nullConnectionId = 0;
            handleIncoming(nullConnectionId, message.data());

            if (mConnection && mConnection->isOpen()) {
                ba::post(*mpIoContext, [this] {
                    startReceiving();
                });
            }
        };

        mConnection->readAsync(onRead);
    }

    void ApiClient::send(const std::string_view message) {
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
