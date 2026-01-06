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
        std::vector<uint8_t> receivedMessage;
        RfTypes::RfCommand commandResponse;

        auto timer = ba::steady_timer(co_await ba::this_coro::executor); //TODO !pr add t/o

        // Handle session engaged by module, await for notification
        if (mMetadata.sessionType == RfTypes::SessionType::FROM_MODULE) {
            RfTypes::RfCommand errorCommand;
            errorCommand.commandType = RfTypes::CommandTypes::NEGATIVE;

            receivedMessage = co_await receive();
            if (receivedMessage.empty()) co_return ""; // return empty if nothing was received

            try {
                commandResponse = RfTypes::RfCommand(receivedMessage);
            } catch (...) {
                // ignore catch - commandResponse.commandType is undefined
            }

            if (commandResponse.commandType == RfTypes::CommandTypes::NOTIFY) {
                try {
                    co_return RfApi::toApiString(commandResponse);
                } catch (...) {
                    // ignore catch - commandResponse is invalid
                }
            }

            co_await send(errorCommand.to_vector()); // Send negative on invalid received command
            co_return ""; // return empty on error
        }

        SmartHome::API::ApiError error;
        SmartHome::API::ApiResponse response;

        // Change channel to target's channel
        bool isChannelChangeSuccessful = co_await changeChannel(mMetadata.rfChannel);
        if (!isChannelChangeSuccessful) {
            error.code = SmartHome::API::ErrorCodes::MEDIATOR_RUNTIME_ERROR;
            error.message = SmartHome::API::errorCodeToString(error.code);
            error.data = "Mediator failed to change rf channel";

            response.error = error;
            response.id = nullptr;

            co_return response.to_string();
        }


        auto previousState = State::NEXT_COMMAND;
        auto state = State::NEXT_COMMAND;

        auto changeState = [&previousState, &state](const State newState) {
            previousState = state;
            state = newState;
        };

        std::vector<uint8_t> lastSendMessage;
        RfTypes::RfCommand currentCommand;
        std::vector<std::string> resultVector;

        uint retries = 0;
        size_t commandsVectorIndex = 0;

        while (state != State::FINISHED) {
            switch (state) {
                case State::SEND_MESSAGE:
                    lastSendMessage = currentCommand.to_vector();
                    co_await send(lastSendMessage);
                    changeState(State::AWAIT_RESPONSE);
                    break;


                case State::AWAIT_RESPONSE:
                    receivedMessage = co_await receive();
                    if (receivedMessage.empty()) {
                        mpLogger->debug("[SESSION] [EXECUTE] [AWAIT_RESPONSE] empty response");
                        changeState(State::RESEND_LAST_MESSAGE);
                        break;
                    }

                    try {
                        commandResponse = RfTypes::RfCommand(receivedMessage);
                    } catch (const std::exception &e) {
                        mpLogger->debugf("[SESSION] [EXECUTE] [AWAIT_RESPONSE] parse to rf command failed: %s",
                                         e.what());
                        changeState(State::SEND_REPEAT_LAST_MESSAGE);
                        break;
                    }

                    // Check if received acknowledge as response to notify
                    if (currentCommand.commandType == RfTypes::CommandTypes::NOTIFY) {
                        if (commandResponse.commandType == RfTypes::CommandTypes::ACKNOWLEDGE) {
                            changeState(State::NEXT_COMMAND);
                            break;
                        }
                        mpLogger->debug("[SESSION] [EXECUTE] [AWAIT_RESPONSE] invalid response for notification");
                        changeState(State::SEND_REPEAT_LAST_MESSAGE);
                        break;
                    }

                    if (commandResponse.commandType != RfTypes::CommandTypes::RESPONSE &&
                        commandResponse.commandType != RfTypes::CommandTypes::PING) {
                        mpLogger->debug("[SESSION] [EXECUTE] [AWAIT_RESPONSE] invalid response type");
                        changeState(State::SEND_REPEAT_LAST_MESSAGE);
                        break;
                    }

                    try {
                        resultVector.push_back(RfApi::toApiString(commandResponse));
                    } catch (const std::exception &e) {
                        mpLogger->debugf("[SESSION] [EXECUTE] [AWAIT_RESPONSE] parse to api response failed: %s",
                                         e.what());
                        changeState(State::SEND_REPEAT_LAST_MESSAGE);
                        break;
                    }

                    changeState(State::NEXT_COMMAND);
                    break;


                case State::RESEND_LAST_MESSAGE:
                    if (++retries > msMAX_REATTEMPTS) {
                        error.code = sa::ErrorCodes::MEDIATOR_COMMUNICATION_ERROR;
                        error.message = SmartHome::API::errorCodeToString(error.code);
                        error.data = "No response from module";

                        response.error = error;
                        response.id = currentCommand.requestId.value();

                        resultVector.push_back(response.to_string());

                        changeState(State::NEXT_COMMAND);
                        break;
                    }
                    co_await send(lastSendMessage);
                    changeState(previousState);
                    break;


                case State::SEND_REPEAT_LAST_MESSAGE: {
                    if (++retries > msMAX_REATTEMPTS) {
                        error.code = sa::ErrorCodes::MEDIATOR_COMMUNICATION_ERROR;
                        error.message = SmartHome::API::errorCodeToString(error.code);
                        error.data = "Module sent invalid response";

                        response.error = error;

                        if (currentCommand.requestId.has_value())
                            response.id = currentCommand.requestId.value();
                        else
                            response.id = nullptr;

                        resultVector.push_back(response.to_string());

                        changeState(State::NEXT_COMMAND);
                        break;
                    }
                    RfTypes::RfCommand command;
                    command.commandType = RfTypes::CommandTypes::REPEAT;
                    co_await send(command.to_vector());

                    changeState(previousState);
                    break;
                }


                case State::SEND_END_COMMAND: {
                    RfTypes::RfCommand command;
                    command.commandType = RfTypes::CommandTypes::END;
                    co_await send(command.to_vector());
                    changeState(State::FINISHED);
                    break;
                }


                case State::FINISHED: break;
                case State::NEXT_COMMAND:
                default: {
                    retries = 0;
                    if (mMetadata.commands.empty()) {
                        changeState(State::SEND_END_COMMAND);
                        break;
                    }

                    if (commandsVectorIndex < mMetadata.commands.size()) {
                        currentCommand = mMetadata.commands[commandsVectorIndex++];

                        if (currentCommand.commandType == RfTypes::CommandTypes::UNDEFINED) {
                            error.code = sa::ErrorCodes::MEDIATOR_COMMUNICATION_ERROR;
                            error.message = SmartHome::API::errorCodeToString(error.code);
                            error.data = "Failed to parse RfCommand";

                            response.error = error;
                            response.id = currentCommand.requestId.value();

                            resultVector.push_back(response.to_string());
                            break;
                        }

                        changeState(State::SEND_MESSAGE);
                        break;
                    }

                    changeState(State::SEND_END_COMMAND);
                    break;
                }
            }
        }

        boost::asio::steady_timer awaitSendTimer(co_await boost::asio::this_coro::executor);

        // Wait until all messages are send
        while (!mpRfClient->isSendQueueEmpty()) {
            awaitSendTimer.expires_after(msPOOLING_DELAY);
            co_await awaitSendTimer.async_wait(ba::use_awaitable);
        }

        awaitSendTimer.expires_after(mpRfDriver->getRequiredWriteDelay() + msPOOLING_DELAY);
        co_await awaitSendTimer.async_wait(ba::use_awaitable);

        // Return to default channel, ignore errors
        co_await changeChannel(mpRfClient->getDefaultChannel());

        auto resultJsonArray = nlohmann::json::array();
        for (const auto &result: resultVector) {
            try {
                resultJsonArray.push_back(nlohmann::json::parse(result));
            } catch (const std::exception &e) {
                mpLogger->errorf("[SESSION] [EXECUTE] Failed to parse result: %s", e.what());
            }
        }
        co_return to_string(resultJsonArray);
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
        mpLogger->debugf("[SESSION] [RECEIVE] Received packet: [%s]", tmp.c_str());
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
}
