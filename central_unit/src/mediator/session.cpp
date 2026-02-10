#include "session.h"
#include "rf_api/rf_client.h"
#include "rf_driver/hc12_driver.h"
#include "rf_api/rf_api.h"
#include "constants.h"

#include <cmath>
#include <utility>


namespace SmartHomeMediator {
    using namespace std::string_literals;
    namespace sc = SmartHome::Constants;

    Session::Session(RfTypes::SessionMetadata &&metadata,
                     const std::shared_ptr<HC12Driver> &pRfDriver,
                     const std::shared_ptr<RfClient> &pRfClient,
                     const std::shared_ptr<su::Logger> &pLogger)
        : mMetadata(std::move(metadata)),
          mpRfDriver(pRfDriver),
          mpRfClient(pRfClient),
          mpLogger(pLogger) {
    }

    ba::awaitable<std::string> Session::execute() {
        mpLogger->debug("[SESSION] [EXECUTE] called");

        if (mMetadata.sessionType == RfTypes::SessionType::MEDIATOR_CONFIG) {
            co_return co_await handleMediatorConfigSession();
        }

        auto resultJsonArray = nlohmann::json::array();

        const bool isInitializedFromModule = mMetadata.sessionType == RfTypes::SessionType::FROM_MODULE;
        bool isSessionCanceled = false;

        auto timeoutTimer = ba::steady_timer(co_await ba::this_coro::executor);
        auto delayTimer = ba::steady_timer(co_await ba::this_coro::executor);

        ExecutionContext ctx(isInitializedFromModule, &delayTimer);

        // Lambda for generating error responses for all commands
        auto generateErrorResponses = [this, &resultJsonArray](SmartHome::API::ApiError &error) {
            SmartHome::API::ApiResponse response;
            for (const auto &pCommand: mMetadata.commands) {
                if (pCommand->getType() != RfTypes::CommandType::RF) {
                    mpLogger->error("[SESSION] [EXECUTE] Received invalid command type ");
                    continue;
                }
                const auto *pRfCommand = dynamic_cast<RfTypes::RfCommand *>(pCommand.get());
                if (!pRfCommand) {
                    mpLogger->error("[SESSION] [EXECUTE] Failed command class cast ");
                    continue;
                }

                if (pRfCommand->requestId.has_value()) response.id = pRfCommand->requestId.value();
                else response.id = nullptr;

                response.error = error;

                try {
                    resultJsonArray.push_back(response.to_json());
                } catch (...) {
                    // Ignore exception
                }
            }
            return resultJsonArray;
        };

        timeoutTimer.expires_after(msSESSION_TIMEOUT);
        timeoutTimer.async_wait([&isSessionCanceled](const boost::system::error_code &ec) {
            if (!ec) isSessionCanceled = true;
        });


        // Change channel to target's channel
        if (!isInitializedFromModule) {
            if (!co_await changeChannel(mMetadata.rfChannel)) {
                ctx.error.code = SmartHome::API::ErrorCodes::MEDIATOR_RUNTIME_ERROR;
                ctx.error.message = SmartHome::API::errorCodeToString(ctx.error.code);
                ctx.error.data = "Mediator failed to change rf channel";

                co_return to_string(generateErrorResponses(ctx.error));
            }

            ctx.error.code = SmartHome::API::ErrorCodes::MEDIATOR_COMMUNICATION_ERROR;
            ctx.error.message = SmartHome::API::errorCodeToString(ctx.error.code);
            ctx.error.data = "Mediator failed to acquire connection, module may be offline";

            // End session if connection can not be acquired
            if (!co_await acquireConnection()) {
                if (const auto pRfClient = mpRfClient.lock()) co_await changeChannel(pRfClient->getRfMainChannel());
                co_return to_string(generateErrorResponses(ctx.error));
            }
        }

        // Run state machine
        while (ctx.currentState != State::FINISHED && !isSessionCanceled) {
            co_await processState(ctx);
        }

        const auto pRfClient = mpRfClient.lock();
        if (!pRfClient) co_return"";

        // Wait until all messages are send
        while (!pRfClient->isSendQueueEmpty()) {
            delayTimer.expires_after(msPOOLING_DELAY);
            co_await delayTimer.async_wait(ba::use_awaitable);
        }

        delayTimer.expires_after(mpRfDriver->getRequiredWriteDelay() + msPOOLING_DELAY);
        co_await delayTimer.async_wait(ba::use_awaitable);

        // Return to default channel, ignore errors
        if (!isInitializedFromModule) co_await changeChannel(pRfClient->getRfMainChannel());


        if (ctx.resultsVector.empty()) co_return "";

        for (const auto &result: ctx.resultsVector) {
            try {
                resultJsonArray.push_back(nlohmann::json::parse(result));
            } catch (const std::exception &e) {
                mpLogger->errorf("[SESSION] [EXECUTE] Failed to parse result: %s", e.what());
            }
        }
        co_return resultJsonArray.empty() ? "" : to_string(resultJsonArray);
    }

    void Session::addReceivedPacket(RfTypes::Packet packet) {
        const auto pRfClient = mpRfClient.lock();
        if (!pRfClient) return;

        if (!packet.isValid() ||
            packet.macAddress != pRfClient->getUniqueNetworkId() ||
            packet.logicAddress != mMetadata.targetLogicAddress)
            return;

        std::scoped_lock lock(mReceiveMutex);
        mReceivedBuffer.insert(mReceivedBuffer.end(), packet.payload.begin(), packet.payload.end());
        if (packet.isLastPacket()) mIsReceivedBufferReady.store(true, std::memory_order::release);
    }

    Session::ExecutionContext::ExecutionContext(const bool initFromModule, ba::steady_timer *timer)
        : isInitializedFromModule(initFromModule), delayTimer(timer) {
        previousState = State::NEXT_COMMAND;
        currentState = isInitializedFromModule ? State::AWAIT_NOTIFICATION : State::NEXT_COMMAND;

        changeState = [this](const State newState) {
            previousState = currentState;
            currentState = newState;
        };
    }

    ba::awaitable<std::string> Session::handleMediatorConfigSession() const {
        auto jsonResponse = nlohmann::json::array();

        for (const auto &pCommand: mMetadata.commands) {
            SmartHome::API::ApiResponse apiResponse;
            SmartHome::API::ApiError apiError;
            apiError.code = SmartHome::API::ErrorCodes::MEDIATOR_RUNTIME_ERROR;
            apiError.message = SmartHome::API::errorCodeToString(apiError.code);

            if (pCommand->requestId.has_value()) {
                apiResponse.id = pCommand->requestId.value();
            } else {
                apiResponse.id = nullptr;
            }

            if (pCommand->getType() != RfTypes::CommandType::CONFIG) {
                mpLogger->error("[SESSION] [CONFIG_SESSION] Config session received command with invalid type");
                apiError.data = "Config session received command with invalid type.";

                apiResponse.error = apiError;
                jsonResponse.push_back(apiResponse.to_json());
                continue;
            }

            auto pConfigCommand = dynamic_cast<RfTypes::MediatorConfigCommand *>(pCommand.get());
            if (!pConfigCommand) {
                mpLogger->error("[SESSION] [CONFIG_SESSION] Failed command class cast");
                apiError.data = "Config session failed cast to command class.";

                apiResponse.error = apiError;
                jsonResponse.push_back(apiResponse.to_json());
                continue;
            }

            std::string_view key = pConfigCommand->commandKey;
            std::string_view value = pConfigCommand->commandValue;

            switch (pConfigCommand->configCommandType) {
                case RfTypes::MediatorConfigCommandType::EXECUTE:
                    // Execute action - return NOT IMPLEMENTED
                    //TODO implement mediator remote action execution (shutdown, restart...)

                    mpLogger->warning("[SESSION] [CONFIG_SESSION] Execute not implemented");
                    apiError.code = SmartHome::API::ErrorCodes::NOT_IMPLEMENTED;
                    apiError.message = SmartHome::API::errorCodeToString(apiError.code);
                    apiError.data = "Mediator execute not implemented";
                    break;
                case RfTypes::MediatorConfigCommandType::GET:
                    // Get all options
                    if (key == sc::MediatorSpecial::ALL_OPTIONS) {
                        try {
                            apiResponse.result = co_await mpRfDriver->getAllOptions();
                        } catch (const std::exception &e) {
                            mpLogger->errorf("[SESSION] [CONFIG_SESSION] Failed to get all options: %s", e.what());
                            apiError.data = e.what();
                        }
                        break;
                    }

                    // Get singular option
                    try {
                        auto result = co_await mpRfDriver->getOption(key.data());
                        apiResponse.result = {result.begin(), result.end()};
                    } catch (const std::exception &e) {
                        mpLogger->errorf("[SESSION] [CONFIG_SESSION] Failed to get (%s) option: %s",
                                         key.data(),
                                         e.what());
                        apiError.data = e.what();
                    }
                    break;
                case RfTypes::MediatorConfigCommandType::SET:
                    // Set option
                    try {
                        co_await mpRfDriver->setOption(key.data(), value.data());

                        apiResponse.result = key.data() + ":"s + value.data();
                    } catch (const std::exception &e) {
                        mpLogger->errorf("[SESSION] [CONFIG_SESSION] Failed to set (%s) option: %s",
                                         key.data(),
                                         e.what());
                        apiError.data = e.what();
                    }
                    break;
                default:
                    // Default return error
                    mpLogger->error("[SESSION] [CONFIG_SESSION] Invalid command type");
                    apiError.data = "Invalid config session command type";

                    apiResponse.error = apiError;
            }
            // Error data was set, prepare error response
            if (!apiError.data.empty()) apiResponse.error = apiError;

            jsonResponse.push_back(apiResponse.to_json());
        }
        co_return jsonResponse.empty() ? "" : to_string(jsonResponse);
    }

    ba::awaitable<void> Session::processState(ExecutionContext &ctx) {
        switch (ctx.currentState) {
            case State::SEND_MESSAGE:
                ctx.lastSendMessage = ctx.pCurrentCommand->to_vector();
                co_await send(ctx.lastSendMessage);
                ctx.changeState(State::AWAIT_RESPONSE);
                break;


            case State::AWAIT_RESPONSE:
                // Await response
                ctx.receivedMessage = co_await receive();
                if (ctx.receivedMessage.empty()) {
                    mpLogger->debug("[SESSION] [EXECUTE] [AWAIT_RESPONSE] empty message");
                    ctx.changeState(State::RESEND_LAST_MESSAGE);
                    break;
                }

                // Try to parse response
                try {
                    ctx.pCommandResponse = std::make_unique<RfTypes::RfCommand>(ctx.receivedMessage);
                } catch (const std::exception &e) {
                    mpLogger->debugf("[SESSION] [EXECUTE] [AWAIT_RESPONSE] parse to rf command failed: %s",
                                     e.what());
                    ctx.changeState(State::SEND_REPEAT_LAST_MESSAGE);
                    break;
                }

                // Check if received acknowledge as response to notify
                if (ctx.pCurrentCommand->rfCommandType == RfTypes::RfCommandType::NOTIFY) {
                    if (ctx.pCommandResponse->rfCommandType == RfTypes::RfCommandType::ACKNOWLEDGE) {
                        ctx.changeState(State::NEXT_COMMAND);
                        break;
                    }
                    mpLogger->debug("[SESSION] [EXECUTE] [AWAIT_RESPONSE] invalid response for notification");
                    ctx.changeState(State::SEND_REPEAT_LAST_MESSAGE);
                    break;
                }

                // Check if response is in response/reping format
                if (ctx.pCommandResponse->rfCommandType != RfTypes::RfCommandType::RESPONSE &&
                    ctx.pCommandResponse->rfCommandType != RfTypes::RfCommandType::REPING) {
                    mpLogger->debug("[SESSION] [EXECUTE] [AWAIT_RESPONSE] invalid response type");
                    ctx.changeState(State::SEND_REPEAT_LAST_MESSAGE);
                    break;
                }

                // Parse RF response to SmartHome::API format
                try {
                    const auto apiString = RfApi::toApiString(*ctx.pCommandResponse);
                    if (!apiString.empty()) ctx.resultsVector.push_back(apiString);
                } catch (const std::exception &e) {
                    mpLogger->debugf("[SESSION] [EXECUTE] [AWAIT_RESPONSE] parse to api response failed: %s",
                                     e.what());
                    ctx.changeState(State::SEND_REPEAT_LAST_MESSAGE);
                    break;
                }

                ctx.changeState(State::NEXT_COMMAND);
                break;


            case State::RESEND_LAST_MESSAGE:
                if (++ctx.retries > msMAX_REATTEMPTS) {
                    ctx.error.code = sa::ErrorCodes::MEDIATOR_COMMUNICATION_ERROR;
                    ctx.error.message = SmartHome::API::errorCodeToString(ctx.error.code);
                    ctx.error.data = "No response from module";

                    ctx.response.error = ctx.error;
                    ctx.response.id = ctx.pCurrentCommand->requestId.value();

                    ctx.resultsVector.push_back(ctx.response.to_string());

                    ctx.changeState(State::NEXT_COMMAND);
                    break;
                }
                co_await send(ctx.lastSendMessage);
                ctx.changeState(ctx.previousState);
                break;


            case State::SEND_REPEAT_LAST_MESSAGE: {
                if (++ctx.retries > msMAX_REATTEMPTS) {
                    if (ctx.isInitializedFromModule) {
                        ctx.changeState(State::SEND_NEG_COMMAND);
                        break;
                    }

                    ctx.error.code = sa::ErrorCodes::MEDIATOR_COMMUNICATION_ERROR;
                    ctx.error.message = SmartHome::API::errorCodeToString(ctx.error.code);
                    ctx.error.data = "Module sent invalid response";

                    ctx.response.error = ctx.error;

                    // Emplace request id in error response if defined
                    if (ctx.pCurrentCommand->requestId.has_value()) {
                        ctx.response.id = ctx.pCurrentCommand->requestId.value();
                    } else {
                        ctx.response.id = nullptr;
                    }

                    ctx.resultsVector.push_back(ctx.response.to_string());

                    ctx.changeState(State::NEXT_COMMAND);
                    break;
                }
                ctx.lowLevelCommand.rfCommandType = RfTypes::RfCommandType::REPEAT;
                co_await send(ctx.lowLevelCommand.to_vector());

                ctx.changeState(ctx.previousState);
                break;
            }


            case State::SEND_END_COMMAND: {
                ctx.lowLevelCommand.rfCommandType = RfTypes::RfCommandType::END;
                co_await send(ctx.lowLevelCommand.to_vector());
                ctx.changeState(State::FINISHED);
                break;
            }

            case State::SEND_ACK_COMMAND: {
                ctx.lowLevelCommand.rfCommandType = RfTypes::RfCommandType::ACKNOWLEDGE;
                co_await send(ctx.lowLevelCommand.to_vector());
                ctx.changeState(State::NEXT_COMMAND);
                break;
            }

            case State::SEND_NEG_COMMAND: {
                ctx.lowLevelCommand.rfCommandType = RfTypes::RfCommandType::NEGATIVE;
                co_await send(ctx.lowLevelCommand.to_vector());
                ctx.changeState(State::NEXT_COMMAND);
                // Wait after sending NEG_COMMAND
                ctx.delayTimer->expires_after(msPOOLING_DELAY * 2);
                co_await ctx.delayTimer->async_wait(ba::use_awaitable);
                break;
            }

            case State::AWAIT_NOTIFICATION:
                // Await notif
                ctx.receivedMessage = co_await receive();
                if (ctx.receivedMessage.empty()) {
                    mpLogger->debug("[SESSION] [EXECUTE] [AWAIT_NOTIFICATION] empty message");
                    ctx.changeState(State::SEND_REPEAT_LAST_MESSAGE);
                    break;
                }

                // Try parsing response
                try {
                    ctx.pCommandResponse = std::make_unique<RfTypes::RfCommand>(ctx.receivedMessage);
                } catch (const std::exception &e) {
                    mpLogger->debugf("[SESSION] [EXECUTE] [AWAIT_NOTIFICATION] parse to rf command failed: %s",
                                     e.what());
                    ctx.changeState(State::SEND_REPEAT_LAST_MESSAGE);
                    break;
                }

                // Check if received notify
                if (ctx.pCommandResponse->rfCommandType != RfTypes::RfCommandType::NOTIFY) {
                    mpLogger->debug("[SESSION] [EXECUTE] [AWAIT_NOTIFICATION] invalid message");
                    ctx.changeState(State::SEND_REPEAT_LAST_MESSAGE);
                    break;
                }

                // Try parsing to SmartHome::API format
                try {
                    const auto apiString = RfApi::toApiString(*ctx.pCommandResponse);
                    if (!apiString.empty()) ctx.resultsVector.push_back(apiString);
                } catch (const std::exception &e) {
                    mpLogger->debugf("[SESSION] [EXECUTE] [AWAIT_NOTIFICATION] parse to api response failed: %s",
                                     e.what());
                    ctx.changeState(State::SEND_REPEAT_LAST_MESSAGE);
                    break;
                }

                ctx.changeState(State::SEND_ACK_COMMAND);
                break;


            case State::FINISHED: break;
            case State::NEXT_COMMAND:
            default: {
                ctx.retries = 0;
                if (mMetadata.commands.empty()) {
                    ctx.changeState(State::SEND_END_COMMAND);
                    break;
                }

                if (ctx.commandsVectorIndex < mMetadata.commands.size()) {
                    ctx.error.code = sa::ErrorCodes::MEDIATOR_COMMUNICATION_ERROR;
                    ctx.error.message = SmartHome::API::errorCodeToString(ctx.error.code);

                    auto &command = mMetadata.commands[ctx.commandsVectorIndex++];
                    // Add error on invalid command type
                    if (command->getType() != RfTypes::CommandType::RF) {
                        ctx.error.data = "Invalid command type in RF session";
                        ctx.response.error = ctx.error;
                        ctx.response.id = ctx.pCurrentCommand->requestId.value();

                        ctx.resultsVector.push_back(ctx.response.to_string());
                        break;
                    }


                    const auto pCommand = dynamic_cast<RfTypes::RfCommand *>(command.release());
                    ctx.pCurrentCommand = std::unique_ptr<RfTypes::RfCommand>(pCommand);

                    // Add error to result on undefined command
                    if (!pCommand || ctx.pCurrentCommand->rfCommandType == RfTypes::RfCommandType::UNDEFINED) {
                        ctx.error.data = "Failed to parse RfCommand";

                        ctx.response.error = ctx.error;
                        ctx.response.id = ctx.pCurrentCommand->requestId.value();

                        ctx.resultsVector.push_back(ctx.response.to_string());
                        break;
                    }

                    ctx.changeState(State::SEND_MESSAGE);
                    break;
                }

                ctx.changeState(State::SEND_END_COMMAND);
                break;
            }
        }
    }

    ba::awaitable<void> Session::send(const std::vector<uint8_t> &message) const {
        const auto maxPayloadSize = RfTypes::Packet::getPayloadMaxSize();
        const auto messageSize = message.size();

        if (messageSize == 0)
            co_return;

        uint8_t numOfPackets = (messageSize + maxPayloadSize - 1) / maxPayloadSize;

        for (auto offset = 0; offset < messageSize; offset += maxPayloadSize) {
            const size_t payloadSize = std::min(static_cast<size_t>(maxPayloadSize), messageSize - offset);

            const auto pRfClient = mpRfClient.lock();
            if (!pRfClient) co_return;

            RfTypes::Packet packet{
                .macAddress = pRfClient->getUniqueNetworkId(),
                .logicAddress = mMetadata.targetLogicAddress,
                .packetsLeft = --numOfPackets,
            };

            std::ranges::fill(packet.payload, RfTypes::Packet::getFillSymbol());
            std::copy_n(message.begin() + offset, payloadSize, packet.payload.begin());

            packet.insertEndMarker();
            packet.insertChecksum();
            pRfClient->addMessageToSend(packet.to_vector());
        }
    }

    ba::awaitable<std::vector<uint8_t> > Session::receive() {
        auto timeoutTimer = ba::steady_timer(co_await ba::this_coro::executor);
        auto poolingTimer = ba::steady_timer(co_await ba::this_coro::executor);

        std::atomic_bool timeout = false;

        auto timeoutCallback = [&timeout, this](const bs::error_code &ec) {
            if (!ec) {
                mpLogger->debug("[SESSION] [RECEIVE] timeout");
                timeout = true;
            }
        };

        timeoutTimer.expires_after(msRECEIVE_MESSAGE_TIMEOUT);
        timeoutTimer.async_wait(timeoutCallback);

        // Wait until buffer is ready or timeout happens
        while (!mIsReceivedBufferReady.load(std::memory_order::relaxed)) {
            if (timeout) {
                mpLogger->debug("[SESSION] [RECEIVE] receive loop timed out");
                std::scoped_lock lock(mReceiveMutex);
                std::string tmp;
                for (const auto &e: mReceivedBuffer) {
                    tmp += std::to_string(e) + ",";
                }
                tmp.pop_back();
                mpLogger->debugf("[SESSION] [RECEIVE] Timed out message: [%s]", tmp.c_str());
                mIsReceivedBufferReady.store(false, std::memory_order::release);
                mReceivedBuffer.clear();
                co_return std::vector<uint8_t>();
            }
            poolingTimer.expires_after(msPOOLING_DELAY);
            co_await poolingTimer.async_wait(ba::use_awaitable);
        }
        timeoutTimer.cancel();

        // Lock mutex for reading from buffer
        std::scoped_lock lock(mReceiveMutex);
        std::vector<uint8_t> result = mReceivedBuffer;
        mReceivedBuffer.clear();
        mIsReceivedBufferReady.store(false, std::memory_order::release);

        // TODO remove before merging with main - left for debug
        if (mpLogger->getLevel() == SmartHome::Utils::LogLevels::Level::DEBUG) {
            std::string tmp;
            for (const auto &e: result) {
                tmp += std::to_string(e) + ",";
            }
            tmp.pop_back();
            mpLogger->debugf("[SESSION] [RECEIVE] Received message: [%s]", tmp.c_str());
        }
        co_return result;
    }

    ba::awaitable<bool> Session::changeChannel(const uint8_t channel) const {
        mpLogger->debug("[SESSION] [CHANGE_CHANNEL] called");
        if (!mpRfDriver->isMultiChannel()) {
            mpLogger->debug("[SESSION] [CHANGE_CHANNEL] isMultiChannel is set to false: skipping change channel");
            co_return true;
        }

        auto retryTimer = ba::steady_timer(co_await ba::this_coro::executor);
        bool isSuccessful = false;
        constexpr int retries = 3;
        for (int i = 0; i < retries; i++) {
            try {
                isSuccessful = co_await mpRfDriver->setOption(sc::MediatorSpecial::CHANNEL.data(),
                                                              std::to_string(channel));
            } catch (const std::exception &e) {
                mpLogger->errorf("[SESSION] [CHANGE_CHANNEL] Failed to change RF channel. Attempt %d/%d. Cause: %s",
                                 i + 1,
                                 retries,
                                 e.what());
            }
            if (isSuccessful) co_return isSuccessful;
            // Wait between retries
            retryTimer.expires_after(100ms);
            co_await retryTimer.async_wait(ba::use_awaitable);
        }

        co_return isSuccessful;
    }

    ba::awaitable<bool> Session::acquireConnection() {
        std::unique_ptr<RfTypes::RfCommand> pCommandResponse;
        RfTypes::RfCommand command;
        command.rfCommandType = RfTypes::RfCommandType::NOTIFY;
        command.requestType.emplace(RfTypes::NotificationType::WAKE);

        auto retries = 0;

        while (++retries <= msMAX_REATTEMPTS) {
            co_await send(command.to_vector());
            std::vector<uint8_t> receivedMessage = co_await receive();

            if (receivedMessage.empty()) continue;

            try {
                pCommandResponse = std::make_unique<RfTypes::RfCommand>(receivedMessage);
            } catch (...) {
                continue;
            }

            if (pCommandResponse->rfCommandType == RfTypes::RfCommandType::ACKNOWLEDGE) {
                co_return true;
            }
        }
        mpLogger->debugf("[SESSION] [ACQUIRE_CONNECTION] Failed to acquire connection");
        co_return false;
    }
}
