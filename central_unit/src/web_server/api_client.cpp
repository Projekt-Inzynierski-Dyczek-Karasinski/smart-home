#include "api_client.h"

#include <boost/algorithm/string/trim.hpp>

SmartHomeWebServer::ApiClient::ApiClient(const std::shared_ptr<su::AsyncLogger> &pLogger, ba::io_context &ioContext)
    : mIoContext(ioContext), mpLogger(pLogger) {
}

SmartHomeWebServer::ApiClient::~ApiClient() {
    mpSocketClient.reset();
}

bool SmartHomeWebServer::ApiClient::initialize(const Config &config) {
    mpSocketClient = std::make_unique<si::SocketClient>(&mIoContext, mpLogger, sc::Targets::WEB_SERVER.data());

    bool isConnectionEstablished = false;
    if (config.uds.isEnabled) {
        isConnectionEstablished = mpSocketClient->connectToServer(config.uds.endpointPath);
        if (isConnectionEstablished)
            mpLogger->info("[WEB_SERVER] [API_CLIENT] Connection established via UDS");
    }
    if (!isConnectionEstablished && config.tcp.isEnabled) {
        isConnectionEstablished = mpSocketClient->connectToServer(config.tcp.endpointAddress,
                                                                  config.tcp.endpointPort);
        if (isConnectionEstablished)
            mpLogger->info("[WEB_SERVER] [API_CLIENT] Connection established via TCP");
    }
    if (!isConnectionEstablished) {
        mpLogger->critical("[WEB_SERVER] [API_CLIENT] Failed to connect with SmartHome daemon");
        return false;
    }

    mpSocketClient->initialize([this](const std::string &message) {
        handleIncoming(message);
    });

    return true;
}

std::future<std::string> SmartHomeWebServer::ApiClient::sendRequest(const sa::ApiRequest &request) {
    std::promise<std::string> promise;
    auto future = promise.get_future();

    // Store promise in pending map with thread safety, when request has an id (is not a notification)
    if (request.id.hasValue()) {
        std::scoped_lock lock(mMutex);
        mPending.emplace(request.id.value(), std::move(promise));


        // Start timeout timer for the request
        auto timer = std::make_shared<ba::steady_timer>(mIoContext, msREQUEST_TIMEOUT);
        timer->async_wait([this, id = request.id.value(), timer](const boost::system::error_code &ec) {
            if (ec) return;
            std::scoped_lock lock(mMutex);
            if (const auto iter = mPending.find(id); iter != mPending.end()) {
                iter->second.set_exception(std::make_exception_ptr(std::runtime_error("Request timeout")));
                mPending.erase(iter);
            }
        });
    } else {
        promise.set_value("");
    }

    static constexpr SmartHome::connectionId_t nullConnId = 0;
    mpSocketClient->handleOutgoing(nullConnId, request.to_string());

    return future;
}

void SmartHomeWebServer::ApiClient::handleIncoming(const std::string &message) {
    // Handle incoming message and fulfill corresponding promise
    mpLogger->debugf("[WEB_SERVER] [API_CLIENT] Received message: %s", message.c_str());
    auto messageTrimmed = boost::algorithm::trim_copy(message);


    nlohmann::json jsonMessage;

    if (nlohmann::json::accept(messageTrimmed)) {
        jsonMessage = nlohmann::json::parse(messageTrimmed);
    } else {
        mpLogger->errorf("[WEB_SERVER] [API_CLIENT]  Received non-JSON message: %s", messageTrimmed.c_str());
        return;
    }

    // Check for incoming responses
    if (jsonMessage.is_array()) {
        // Handle responses from JSON batch, leave potential requests in jsonMessage to be processed below
        for (auto i = std::ssize(jsonMessage) - 1; i >= 0; --i) {
            // Iterate backwards to allow erasing processed responses without affecting unprocessed items
            const auto &candidate = jsonMessage[i];
            const bool hasMethod = candidate.contains(SmartHome::JsonRpcStrings::RequestKeys::METHOD);
            const bool hasResult = candidate.contains(SmartHome::JsonRpcStrings::ResponseKeys::RESULT);
            const bool hasError = candidate.contains(SmartHome::JsonRpcStrings::ResponseKeys::ERROR);

            if (!hasMethod && (hasResult || hasError)) {
                try {
                    SmartHome::API::ApiResponse response(candidate);
                    ba::post(mIoContext, [this, response] {
                        handleIncomingResponse(response);
                    });
                    jsonMessage.erase(jsonMessage.begin() + i);
                } catch (const std::exception &e) {
                    mpLogger->debugf("[WEB_SERVER] [API_CLIENT] Parse to response failed: %s", e.what());
                }
            }
        }
        if (jsonMessage.empty()) return;
    } else {
        const bool hasMethod = jsonMessage.contains(SmartHome::JsonRpcStrings::RequestKeys::METHOD);
        const bool hasResult = jsonMessage.contains(SmartHome::JsonRpcStrings::ResponseKeys::RESULT);
        const bool hasError = jsonMessage.contains(SmartHome::JsonRpcStrings::ResponseKeys::ERROR);

        if (!hasMethod && (hasResult || hasError)) {
            try {
                auto response = SmartHome::API::ApiResponse(jsonMessage);
                ba::post(mIoContext, [this, response] {
                    handleIncomingResponse(response);
                });
                return;
            } catch (const std::exception &e) {
                mpLogger->debugf("[WEB_SERVER] [API_CLIENT]  Parse to response failed: %s", e.what());
            }
        }
    }

    static constexpr SmartHome::connectionId_t nullConnId = 0;
    SmartHome::API::ApiResponse errorResponse;
    errorResponse.id = nullptr;

    SmartHome::API::ApiError e;
    e.code = SmartHome::API::ErrorCodes::PARSE_ERROR;
    e.message = SmartHome::API::errorCodeToString(e.code);
    if (jsonMessage.is_array()) {
        // Parse JSON-RPC batch request
        for (const auto &unpackedRequest: jsonMessage) {
            try {
                auto request = SmartHome::API::ApiRequest(unpackedRequest);
                ba::post(mIoContext, [this, request] {
                    handleIncomingRequest(request);
                });
            } catch (const std::exception &exception) {
                e.data = exception.what();
                errorResponse.error = e;
                mpSocketClient->handleOutgoing(nullConnId, errorResponse.to_string());

                mpLogger->error(
                    "[WEB_SERVER] [API_CLIENT]  JSON-RPC batch request parse error: " + std::string(exception.what()));
            }
        }
        return;
    }

    // Parse singular JSON-RPC request
    try {
        auto request = SmartHome::API::ApiRequest(jsonMessage);
        ba::post(mIoContext, [this, request] {
            handleIncomingRequest(request);
        });
    } catch (const std::exception &exception) {
        e.data = exception.what();
        errorResponse.error = e;
        mpSocketClient->handleOutgoing(nullConnId, errorResponse.to_string());

        mpLogger->error(
            "[WEB_SERVER] [API_CLIENT]  JSON-RPC request parse error: " + std::string(exception.what()));
    }
}

void SmartHomeWebServer::ApiClient::handleIncomingResponse(const SmartHome::API::ApiResponse &response) {
    mpLogger->debugf("[WEB_SERVER] [API_CLIENT] Handling incoming response: %s", response.to_string().c_str());
    if (!response.id.hasValue()) {
        mpLogger->error("[WEB_SERVER] [API_CLIENT] Received response without id");
        return;
    }

    std::promise<std::string> promise;
    // Retrieve and remove the corresponding promise from the pending map with thread safety
    {
        std::scoped_lock lock(mMutex);
        const auto iter = mPending.find(response.id.value());
        if (iter != mPending.end()) {
            promise = std::move(iter->second);
            mPending.erase(iter);
        } else {
            mpLogger->errorf("[WEB_SERVER] [API_CLIENT] Received response with unknown id: %u", response.id.value());
            return;
        }
    }

    promise.set_value(response.to_string());
}

void SmartHomeWebServer::ApiClient::handleIncomingRequest(const SmartHome::API::ApiRequest &request) {
    mpLogger->debugf("[WEB_SERVER] [API_CLIENT] Handling incoming request: %s", request.to_string().c_str());

    mpLogger->warningf("[WEB_SERVER] [API_CLIENT] Requests not implemented, received: %s", request.to_string().c_str());
}
