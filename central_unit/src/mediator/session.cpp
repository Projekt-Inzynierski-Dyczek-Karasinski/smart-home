#include "session.h"
#include "rf_driver/hc12_driver.h"
#include "rf_client.h"
#include "rf_api.h"

#include <charconv>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <utility>


namespace SmartHomeMediator {
    Session::Session(RfTypes::SessionMetadata metadata,
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

        auto resultJsonArray = nlohmann::json::array();

        const bool isInitializedFromModule = mMetadata.sessionType == RfTypes::SessionType::FROM_MODULE;
        bool isSessionCanceled = false;

        auto timeoutTimer = ba::steady_timer(co_await ba::this_coro::executor);
        auto delayTimer = ba::steady_timer(co_await ba::this_coro::executor);

        ExecutionContext ctx(isInitializedFromModule, &delayTimer);

        // Lambda for generating error responses for all commands
        auto generateErrorResponses = [this, &resultJsonArray](SmartHome::API::ApiError &error) {
            SmartHome::API::ApiResponse response;
            for (const auto &command: mMetadata.commands) {
                if (command.requestId.has_value()) response.id = command.requestId.value();
                else response.id = nullptr;

                response.error = error;

                try {
                    resultJsonArray.push_back(response.to_json());
                } catch (...) {
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
            if (!co_await acquireConnection()) co_return to_string(generateErrorResponses(ctx.error));
        }

        // Run state machine
        while (ctx.state != State::FINISHED && !isSessionCanceled) {
            co_await processState(ctx);
        }

        // Wait until all messages are send
        while (!mpRfClient->isSendQueueEmpty()) {
            delayTimer.expires_after(msPOOLING_DELAY);
            co_await delayTimer.async_wait(ba::use_awaitable);
        }

        delayTimer.expires_after(mpRfDriver->getRequiredWriteDelay() + msPOOLING_DELAY);
        co_await delayTimer.async_wait(ba::use_awaitable);

        // Return to default channel, ignore errors
        if (!isInitializedFromModule) co_await changeChannel(mpRfClient->getDefaultChannel());


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
        if (!packet.isValid() ||
            packet.macAddress != mpRfClient->getDefaultMacAddress() ||
            packet.logicAddress != mMetadata.targetLogicAddress)
            return;

        std::scoped_lock lock(mReceiveMutex);
        mReceivedBuffer.insert(mReceivedBuffer.end(), packet.payload.begin(), packet.payload.end());
        if (packet.isLastPacket()) mIsReceivedBufferReady = true;
    }

    ba::awaitable<void> Session::processState(ExecutionContext &ctx) {
        switch (ctx.state) {
            case State::SEND_MESSAGE:
                ctx.lastSendMessage = ctx.currentCommand.to_vector();
                co_await send(ctx.lastSendMessage);
                ctx.changeState(State::AWAIT_RESPONSE);
                break;


            case State::AWAIT_RESPONSE:
                ctx.receivedMessage = co_await receive();
                if (ctx.receivedMessage.empty()) {
                    mpLogger->debug("[SESSION] [EXECUTE] [AWAIT_RESPONSE] empty message");
                    ctx.changeState(State::RESEND_LAST_MESSAGE);
                    break;
                }

                try {
                    ctx.commandResponse = RfTypes::RfCommand(ctx.receivedMessage);
                } catch (const std::exception &e) {
                    mpLogger->debugf("[SESSION] [EXECUTE] [AWAIT_RESPONSE] parse to rf command failed: %s",
                                     e.what());
                    ctx.changeState(State::SEND_REPEAT_LAST_MESSAGE);
                    break;
                }

                // Check if received acknowledge as response to notify
                if (ctx.currentCommand.commandType == RfTypes::CommandTypes::NOTIFY) {
                    if (ctx.commandResponse.commandType == RfTypes::CommandTypes::ACKNOWLEDGE) {
                        ctx.changeState(State::NEXT_COMMAND);
                        break;
                    }
                    mpLogger->debug("[SESSION] [EXECUTE] [AWAIT_RESPONSE] invalid response for notification");
                    ctx.changeState(State::SEND_REPEAT_LAST_MESSAGE);
                    break;
                }

                if (ctx.commandResponse.commandType != RfTypes::CommandTypes::RESPONSE &&
                    ctx.commandResponse.commandType != RfTypes::CommandTypes::REPING) {
                    mpLogger->debug("[SESSION] [EXECUTE] [AWAIT_RESPONSE] invalid response type");
                    ctx.changeState(State::SEND_REPEAT_LAST_MESSAGE);
                    break;
                }

                try {
                    ctx.resultsVector.push_back(RfApi::toApiString(ctx.commandResponse));
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
                    ctx.response.id = ctx.currentCommand.requestId.value();

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

                    if (ctx.currentCommand.requestId.has_value())
                        ctx.response.id = ctx.currentCommand.requestId.value();
                    else
                        ctx.response.id = nullptr;

                    ctx.resultsVector.push_back(ctx.response.to_string());

                    ctx.changeState(State::NEXT_COMMAND);
                    break;
                }
                ctx.lowLevelCommand.commandType = RfTypes::CommandTypes::REPEAT;
                co_await send(ctx.lowLevelCommand.to_vector());

                ctx.changeState(ctx.previousState);
                break;
            }


            case State::SEND_END_COMMAND: {
                ctx.lowLevelCommand.commandType = RfTypes::CommandTypes::END;
                co_await send(ctx.lowLevelCommand.to_vector());
                ctx.changeState(State::FINISHED);
                break;
            }

            case State::SEND_ACK_COMMAND: {
                ctx.lowLevelCommand.commandType = RfTypes::CommandTypes::ACKNOWLEDGE;
                co_await send(ctx.lowLevelCommand.to_vector());
                ctx.changeState(State::NEXT_COMMAND);
                break;
            }

            case State::SEND_NEG_COMMAND: {
                ctx.lowLevelCommand.commandType = RfTypes::CommandTypes::NEGATIVE;
                co_await send(ctx.lowLevelCommand.to_vector());
                ctx.changeState(State::NEXT_COMMAND);
                ctx.delayTimer->expires_after(msPOOLING_DELAY);
                co_await ctx.delayTimer->async_wait(ba::use_awaitable);
                break;
            }

            case State::AWAIT_NOTIFICATION:
                ctx.receivedMessage = co_await receive();
                if (ctx.receivedMessage.empty()) {
                    mpLogger->debug("[SESSION] [EXECUTE] [AWAIT_NOTIFICATION] empty message");
                    ctx.changeState(State::SEND_REPEAT_LAST_MESSAGE);
                    break;
                }

                try {
                    ctx.commandResponse = RfTypes::RfCommand(ctx.receivedMessage);
                } catch (const std::exception &e) {
                    mpLogger->debugf("[SESSION] [EXECUTE] [AWAIT_NOTIFICATION] parse to rf command failed: %s",
                                     e.what());
                    ctx.changeState(State::SEND_REPEAT_LAST_MESSAGE);
                    break;
                }

                // Check if received notify
                if (ctx.commandResponse.commandType != RfTypes::CommandTypes::NOTIFY) {
                    mpLogger->debug("[SESSION] [EXECUTE] [AWAIT_NOTIFICATION] invalid message");
                    ctx.changeState(State::SEND_REPEAT_LAST_MESSAGE);
                    break;
                }

                try {
                    ctx.resultsVector.push_back(RfApi::toApiString(ctx.commandResponse));
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
                    ctx.currentCommand = mMetadata.commands[ctx.commandsVectorIndex++];

                    // Add error to result on undefined command
                    if (ctx.currentCommand.commandType == RfTypes::CommandTypes::UNDEFINED) {
                        ctx.error.code = sa::ErrorCodes::MEDIATOR_COMMUNICATION_ERROR;
                        ctx.error.message = SmartHome::API::errorCodeToString(ctx.error.code);
                        ctx.error.data = "Failed to parse RfCommand";

                        ctx.response.error = ctx.error;
                        ctx.response.id = ctx.currentCommand.requestId.value();

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

            RfTypes::Packet packet{
                .macAddress = mpRfClient->getDefaultMacAddress(),
                .logicAddress = mMetadata.targetLogicAddress,
                .packetsLeft = --numOfPackets,
            };

            std::ranges::fill(packet.payload, RfTypes::Packet::getFillSymbol());
            std::copy_n(message.begin() + offset, payloadSize, packet.payload.begin());

            packet.insertEndMarker();
            packet.insertChecksum();
            mpRfClient->addMessageToSend(packet.to_vector());
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

        while (!mIsReceivedBufferReady) {
            if (timeout) {
                mpLogger->debug("[SESSION] [RECEIVE] receive loop timed out");
                std::scoped_lock lock(mReceiveMutex);
                std::string tmp;
                for (const auto &e: mReceivedBuffer) {
                    tmp += std::to_string(e) + ",";
                }
                tmp.pop_back();
                mpLogger->debugf("[SESSION] [RECEIVE] Timed out message: [%s]", tmp.c_str());
                mIsReceivedBufferReady = false;
                mReceivedBuffer.clear();
                co_return std::vector<uint8_t>();
            }
            poolingTimer.expires_after(msPOOLING_DELAY);
            co_await poolingTimer.async_wait(ba::use_awaitable);
        }
        timeoutTimer.cancel();

        std::scoped_lock lock(mReceiveMutex);
        std::vector<uint8_t> result = mReceivedBuffer;
        mReceivedBuffer.clear();
        mIsReceivedBufferReady = false;
        std::string tmp;

        for (const auto &e: result) {
            tmp += std::to_string(e) + ",";
        }
        tmp.pop_back();
        mpLogger->debugf("[SESSION] [RECEIVE] Received message: [%s]", tmp.c_str());
        co_return result;
    }

    ba::awaitable<bool> Session::changeChannel(const uint8_t channel) const {
        mpLogger->debug("[SESSION] [CHANGE_CHANNEL] called");
        auto retryTimer = ba::steady_timer(co_await ba::this_coro::executor);
        bool isSuccessful = false;
        for (int i = 0; i < 3; i++) {
            isSuccessful = co_await mpRfDriver->setOption(HC12Driver::Hc12Option::CHANNEL,
                                                          std::to_string(channel));
            if (isSuccessful) co_return isSuccessful;
            retryTimer.expires_after(100ms);
            co_await retryTimer.async_wait(ba::use_awaitable);
        }

        co_return isSuccessful;
    }

    ba::awaitable<bool> Session::acquireConnection() {
        RfTypes::RfCommand commandResponse;
        RfTypes::RfCommand command;
        command.commandType = RfTypes::CommandTypes::NOTIFY;
        command.requestType.emplace(RfTypes::NotificationTypes::WAKE);

        auto retries = 0;

        while (++retries < msMAX_REATTEMPTS) {
            co_await send(command.to_vector());
            std::vector<uint8_t> receivedMessage = co_await receive();

            if (receivedMessage.empty()) continue;

            try {
                commandResponse = RfTypes::RfCommand(receivedMessage);
            } catch (...) {
                continue;
            }

            if (commandResponse.commandType == RfTypes::CommandTypes::ACKNOWLEDGE) {
                co_return true;
            }
        }
        mpLogger->debugf("[SESSION] [ACQUIRE_CONNECTION] Failed to acquire connection");
        co_return false;
    }
}
