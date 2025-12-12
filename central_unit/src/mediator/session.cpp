#include "session.h"
#include "rf_driver/hc12_driver.h"
#include "rf_client.h"

#include <charconv>
#include <cmath>
#include <cstring>
#include <stdexcept>


namespace SmartHomeMediator {
    Packet Packet::from_bytes(const std::span<const uint8_t> data) {
        if (data.size() != sizeof(Packet)) {
            throw std::invalid_argument("Invalid size");
        }

        Packet packet{};
        packet = std::bit_cast<Packet>(data); // TODO !pr test
        // std::memcpy(&packet, data.data(), data.size());
        return packet;
    }

    Packet Packet::from_vector(const std::vector<uint8_t> &data) {
        return from_bytes(data);
    }

    std::span<const uint8_t> Packet::as_bytes() const {
        return {reinterpret_cast<const uint8_t *>(this), sizeof(*this)};
    }

    std::vector<uint8_t> Packet::to_vector() const {
        auto bytes = as_bytes();
        return {bytes.begin(), bytes.end()};
    }

    bool Packet::isValid() const {
        return endMarker == msEND_MARKER && verifyChecksum(*this);
    }

    bool Packet::isLastPacket() const {
        return packetsLeft == 0;
    }

    // std::vector<uint8_t> Packet::getPayload() const {
    //     return {payload.begin(), payload.end()};
    // }

    uint8_t Packet::getPayloadMaxSize() {
        return msPAYLOAD_MAX_SIZE;
    }

    uint8_t Packet::getEndMarker() {
        return msEND_MARKER;
    }

    uint8_t Packet::getFillSymbol() {
        return msFILL_SYMBOL;
    }

    void Packet::insertChecksum() {
        checksum = 0;
        const auto tmpChecksum = calculateChecksum(*this);

        checksum = (msCHECKSUM_MODULO - (tmpChecksum % msCHECKSUM_MODULO)) % msCHECKSUM_MODULO;
    }

    void Packet::insertEndMarker() {
        endMarker = msEND_MARKER;
    }

    uint16_t Packet::calculateChecksum(const Packet &packet) {
        uint16_t tmpChecksum = 0;
        for (const auto &byte: packet.as_bytes()) {
            tmpChecksum += byte;
        }
        return tmpChecksum;
    }

    bool Packet::verifyChecksum(const Packet &packet) {
        return calculateChecksum(packet) % msCHECKSUM_MODULO == 0;
    }

    Session::Session(const Metadata &metadata,
                     const std::shared_ptr<HC12Driver> &rfDriver,
                     const std::shared_ptr<RfClient> &rfClient)
        : mMetadata(metadata),
          mpRfDriver(rfDriver),
          mpRfClient(rfClient) {
    }

    ba::awaitable<std::string> Session::execute() {
        std::vector<uint8_t> receivedMessage;
        RfApi::RfCommand commandResponse;


        auto timer = ba::steady_timer(co_await ba::this_coro::executor); //TODO !pr add to

        // Handle session engaged by module, await for notification
        if (mMetadata.sessionType == Type::FROM_MODULE) {
            RfApi::RfCommand errorCommand;
            errorCommand.commandType = RfApi::CommandTypes::NEGATIVE;

            receivedMessage = co_await receive();
            if (receivedMessage.empty()) co_return ""; // return empty if nothing was received

            try {
                commandResponse = RfApi::RfCommand(receivedMessage);
            } catch (...) {
                // ignore catch - commandResponse.commandType is undefined
            }

            if (commandResponse.commandType == RfApi::CommandTypes::NOTIFY) {
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
        RfApi::RfCommand currentCommand;
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
                        changeState(State::RESEND_LAST_MESSAGE);
                        break;
                    }

                    try {
                        commandResponse = RfApi::RfCommand(receivedMessage);
                    } catch (...) {
                        changeState(State::SEND_REPEAT_LAST_MESSAGE);
                        break;
                    }

                    // Check if received acknowledge as response to notify
                    if (currentCommand.commandType == RfApi::CommandTypes::NOTIFY) {
                        if (commandResponse.commandType == RfApi::CommandTypes::ACKNOWLEDGE) {
                            changeState(State::NEXT_COMMAND);
                            break;
                        }
                        changeState(State::SEND_REPEAT_LAST_MESSAGE);
                        break;
                    }

                    if (commandResponse.commandType != RfApi::CommandTypes::RESPONSE) {
                        changeState(State::SEND_REPEAT_LAST_MESSAGE);
                        break;
                    }

                    resultVector.push_back(RfApi::toApiString(commandResponse));

                    changeState(State::NEXT_COMMAND);
                    break;


                case State::RESEND_LAST_MESSAGE:
                    if (retries++ >= msMAX_REATTEMPTS) {
                        error.code = sa::ErrorCodes::MEDIATOR_COMMUNICATION_ERROR;
                        error.message = SmartHome::API::errorCodeToString(error.code);
                        error.data = "No response from module";

                        response.error = error;
                        response.id = currentCommand.requestId.value_or(nullptr);

                        resultVector.push_back(response.to_string());

                        changeState(State::NEXT_COMMAND);
                        break;
                    }
                    co_await send(lastSendMessage);
                    changeState(previousState);
                    break;


                case State::SEND_REPEAT_LAST_MESSAGE: {
                    if (retries++ >= msMAX_REATTEMPTS) {
                        error.code = sa::ErrorCodes::MEDIATOR_COMMUNICATION_ERROR;
                        error.message = SmartHome::API::errorCodeToString(error.code);
                        error.data = "Module sent invalid response";

                        response.error = error;
                        response.id = currentCommand.requestId.value_or(nullptr);

                        resultVector.push_back(response.to_string());

                        changeState(State::NEXT_COMMAND);
                        break;
                    }
                    RfApi::RfCommand command;
                    command.commandType = RfApi::CommandTypes::REPEAT;
                    co_await send(command.to_vector());

                    changeState(previousState);
                    break;
                }


                case State::SEND_END_COMMAND: {
                    RfApi::RfCommand command;
                    command.commandType = RfApi::CommandTypes::END;
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
                        changeState(State::SEND_MESSAGE);
                        break;
                    }

                    changeState(State::SEND_END_COMMAND);
                    break;
                }
            }
        }

        // Return to default channel, ignore errors
        co_await changeChannel(mpRfClient->getDefaultChannel());

        auto resultJsonArray = nlohmann::json::array();
        resultJsonArray = resultVector;
        co_return to_string(resultJsonArray);
    }

    void Session::addReceivedPacket(Packet packet) {
        if (!packet.isValid() ||
            packet.macAddress != mpRfClient->getDefaultMacAddress() ||
            packet.logicAddress != mMetadata.targetLogicAddress)
            return;

        std::scoped_lock lock(mReceiveMutex);
        mReceivedBuffer.insert(mReceivedBuffer.end(), packet.payload.begin(), packet.payload.end());
        if (packet.isLastPacket()) mIsReceivedBufferReady = true;
    }

    ba::awaitable<void> Session::send(const std::vector<uint8_t> &message) const {
        const auto maxPayloadSize = Packet::getPayloadMaxSize();
        const auto messageSize = message.size();

        if (messageSize == 0)
            co_return;

        uint8_t numOfPackets = (messageSize + maxPayloadSize - 1) / maxPayloadSize;

        for (auto offset = 0; offset < messageSize; offset += maxPayloadSize) {
            const size_t payloadSize = std::min(static_cast<size_t>(maxPayloadSize), messageSize - offset);

            Packet packet{
                .macAddress = mpRfClient->getDefaultMacAddress(),
                .logicAddress = mMetadata.targetLogicAddress,
                .packetsLeft = --numOfPackets,
            };

            std::ranges::fill(packet.payload, Packet::getFillSymbol());
            std::copy_n(message.begin() + offset, payloadSize, packet.payload);

            packet.insertEndMarker();
            packet.insertChecksum();
            mpRfClient->addMessageToSend(packet.to_vector());
        }
    }

    ba::awaitable<std::vector<uint8_t> > Session::receive() {
        auto timeoutTimer = ba::steady_timer(co_await ba::this_coro::executor);
        auto poolingTimer = ba::steady_timer(co_await ba::this_coro::executor);

        std::atomic_bool timeout = false;

        auto timeoutCallback = [&timeout](const bs::error_code &ec) {
            if (!ec) {
                timeout = true;
            }
        };

        timeoutTimer.expires_after(msRECEIVE_MESSAGE_TIMEOUT);
        timeoutTimer.async_wait(timeoutCallback);

        while (!mIsReceivedBufferReady) {
            if (timeout) {
                std::scoped_lock lock(mReceiveMutex);
                mIsReceivedBufferReady = false;
                mReceivedBuffer.clear();
                co_return std::vector<uint8_t>();
            }
            poolingTimer.expires_after(msPOOLING_DELAY);
            co_await poolingTimer.async_wait(ba::use_awaitable);
        }

        std::scoped_lock lock(mReceiveMutex);
        std::vector<uint8_t> result = mReceivedBuffer;
        mReceivedBuffer.clear();
        mIsReceivedBufferReady = false;
        co_return result;
    }

    ba::awaitable<bool> Session::changeChannel(const uint8_t channel) const {
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
