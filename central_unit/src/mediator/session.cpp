#include "session.h"
#include "rf_driver/hc12_driver.h"

#include <charconv>
#include <cmath>
#include <cstring>
#include <stdexcept>

#include "rf_client.h"

namespace SmartHomeMediator {
    Packet Packet::from_bytes(const std::span<const uint8_t> data) {
        if (data.size() != sizeof(Packet)) {
            throw std::invalid_argument("Invalid size");
        }

        Packet packet{};
        std::memcpy(&packet, data.data(), data.size());
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

    ba::awaitable<void> Session::send(std::vector<uint8_t> message) {
        // TODO separate message into packets and send them via RfClient send
    }

    ba::awaitable<std::vector<uint8_t> > Session::receive() {
        // TODO read packets saved via Session::addReceivedPacket and join them
        // TODO !pr return only joined payload (raw RfCommand) wait for all of the packets
        // add timeout - return empty vector on t/o
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

    // uint8_t Session::joinHalfBytesIntoByte(const uint8_t value1, const uint8_t value2) {
    //     return value1 << 4 | value2;
    // }
    //
    // uint8_t Session::getSpecialByte(RfCommands command, const uint8_t numberOfParameters) {
    //     return joinHalfBytesIntoByte(static_cast<uint8_t>(command), numberOfParameters);
    // }
    //
    // uint8_t Session::getSpecialByte(RfCommandsParameterTypes parameterType, const uint8_t parameterSize) {
    //     return joinHalfBytesIntoByte(static_cast<uint8_t>(parameterType), parameterSize);
    // }
    //
    // std::pair<uint8_t, uint8_t> Session::parseSpecialByte(const uint8_t specialByte) {
    //     //TODO !pr test
    //     uint8_t first = specialByte & 0xF0 >> 4;
    //     uint8_t second = specialByte & 0x0F;
    //     return {first, second};
    // }

    Session::Session(const Metadata &metadata, const std::shared_ptr<HC12Driver> &rfDriver)
        : mMetadata(metadata),
          mpRfDriver(rfDriver) {
    }

    ba::awaitable<std::string> Session::execute() {
        std::string result;
        auto timer = ba::steady_timer(co_await ba::this_coro::executor);

        // Change channel to target's channel
        bool isChannelChangeSuccessful = co_await changeChannel(mMetadata.rfChannel);
        if (!isChannelChangeSuccessful) {
            // TODO return error
        }

        // TODO !pr add module initialized session handling
        State previousState = State::NEXT_COMMAND;
        State state = State::NEXT_COMMAND;

        auto changeState = [&previousState, &state](const State newState) {
            previousState = state;
            state = newState;
        };

        RfApi::RfCommand currentCommand;
        RfApi::RfCommand commandResponse;
        std::vector<uint8_t> lastSendMessage;
        std::vector<uint8_t> receivedMessage;

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
                    } catch (const std::exception &e) {
                        // TODO handle error
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

                    // TODO iterate trough parameters adding them to response along with request ID, no ID -> error
                    changeState(State::NEXT_COMMAND);
                    break;


                case State::RESEND_LAST_MESSAGE:
                    if (retries++ >= ms_MAX_REATTEMPTS) {
                        // TODO Add error response
                        changeState(State::NEXT_COMMAND);
                        break;
                    }
                    co_await send(lastSendMessage);
                    changeState(previousState);
                    break;


                case State::SEND_REPEAT_LAST_MESSAGE: {
                    if (retries++ >= ms_MAX_REATTEMPTS) {
                        // TODO Add error response
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


        // Return to default channel
        isChannelChangeSuccessful = co_await changeChannel(mMetadata.rfChannel);
        if (!isChannelChangeSuccessful) {
            // TODO return error
        }

        co_return result;
    }

    void Session::addReceivedPacket(Packet packet) {
        // TODO verify and save packet
    }


    // Session::Session(const uint8_t logicAddress,
    //                  const std::string_view macAddress,
    //                  const std::string_view message)
    //     : mLogicAddress(logicAddress),
    //       mMacAddress(stringMacToArray(macAddress)),
    //       mMessage(message.begin(), message.end()) {
    //     mPacketsLeft = std::ceil(static_cast<double>(mMessage.size()) / Packet::getPayloadMaxSize());
    // }
    //
    // Session::Session(const Packet &packet)
    //     : mLogicAddress(packet.logicAddress),
    //       mMacAddress(packet.macAddress) {
    //     addIncomingPacket(packet);
    // }
    //
    //
    // std::optional<Packet> Session::getOutgoingPacket() {
    //     if (mPacketsLeft == 0) return std::nullopt;
    //     return createNextPacket();
    // }
    //
    // void Session::addIncomingPacket(const Packet &packet) {
    //     mMessage.insert(mMessage.end(), packet.payload.begin(), packet.payload.end());
    // }
    //
    //
    // Packet Session::createNextPacket() {
    //     const auto &payloadSize = Packet::getPayloadMaxSize();
    //     const std::span<const uint8_t> messageSpan = {mMessage.data(), mMessage.size()};
    //     const auto bytesToCopy = std::min(static_cast<size_t>(payloadSize), mMessage.size() - mMessageOffset);
    //
    //     Packet packet{};
    //     packet.logicAddress = mLogicAddress;
    //     packet.macAddress = mMacAddress;
    //     packet.packetsLeft = --mPacketsLeft;
    //
    //     packet.payload.fill(Packet::getFillSymbol());
    //     std::memcpy(packet.payload.data(), messageSpan.data() + mMessageOffset, bytesToCopy);
    //     mMessageOffset += bytesToCopy;
    //
    //     packet.insertEndMarker();
    //     packet.insertChecksum();
    //
    //     return packet;
    // }
    //
    // std::array<uint8_t, 6> Session::stringMacToArray(const std::string_view macAddress) {
    //     static constexpr auto sBASE = 16;
    //     std::array<uint8_t, 6> macArray{};
    //
    //
    //     for (int i = 0; i < 6; i++) {
    //         int offset = i * 3;
    //
    //         auto [_, ec] = std::from_chars(
    //             macAddress.data() + offset,
    //             macAddress.data() + offset + 2,
    //             macArray[i],
    //             sBASE
    //         );
    //
    //         if (ec != std::errc{}) {
    //             throw std::invalid_argument("Invalid MAC address format");
    //         }
    //     }
    //
    //     return macArray;
    // }

    // std::string Transmission::arrayMacToString(const std::array<uint8_t, 6> &macAddress) {
    //     char buffer[18];
    //     std::sprintf(buffer, "%02X:%02X:%02X:%02X:%02X:%02X",
    //                  macAddress[0], macAddress[1], macAddress[2], macAddress[3], macAddress[4], macAddress[5]);
    //     return std::string(buffer);
    // }
}
