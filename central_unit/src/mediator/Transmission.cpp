#include "Transmission.h"

#include <charconv>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace SmartHomeMediator {
    std::span<const uint8_t> Packet::as_bytes() const {
        return {reinterpret_cast<const uint8_t *>(this), sizeof(*this)};
    }

    Packet Packet::from_bytes(const std::span<const uint8_t> data) const {
        if (data.size() != sizeof(*this)) {
            throw std::invalid_argument("Invalid size");
        }

        Packet packet;
        std::memcpy(&packet, data.data(), data.size());
        return packet;
    }

    std::vector<uint8_t> Packet::to_vector() const {
        auto bytes = as_bytes();
        return {bytes.begin(), bytes.end()};
    }

    Packet Packet::from_vector(const std::vector<uint8_t> &data) const {
        return from_bytes(data);
    }

    bool Packet::isValid() const {
        return endMarker == ms_END_MARKER && verifyChecksum(*this);
    }

    bool Packet::isLastPacket() const {
        return packetsLeft == 0;
    }

    // std::vector<uint8_t> Packet::getPayload() const {
    //     return {payload.begin(), payload.end()};
    // }

    uint8_t Packet::getPayloadMaxSize() {
        return ms_PAYLOAD_MAX_SIZE;
    }

    uint16_t Packet::getEndMarker() {
        return ms_END_MARKER;
    }

    void Packet::updateChecksum() {
        checksum = 0;
        const auto tmpChecksum = calculateChecksum(*this);

        checksum = (ms_CHECKSUM_MODULO - (tmpChecksum % ms_CHECKSUM_MODULO)) % ms_CHECKSUM_MODULO;
    }

    void Packet::updateEndMarker() {
        endMarker = ms_END_MARKER;
    }

    uint16_t Packet::calculateChecksum(const Packet &packet) {
        uint16_t tmpChecksum = 0;
        for (const auto &byte: packet.as_bytes()) {
            tmpChecksum += byte;
        }
        return tmpChecksum;
    }

    bool Packet::verifyChecksum(const Packet &packet) {
        return calculateChecksum(packet) % ms_CHECKSUM_MODULO == 0;
    }

    Transmission::Transmission(const uint8_t logicAddress,
                               const std::string_view macAddress,
                               const std::string_view message)
        : mLogicAddress(logicAddress),
          mMacAddress(stringMacToArray(macAddress)),
          mType(Type::OUTGOING),
          mMessage(message.begin(), message.end()) {
        mPacketsLeft = std::ceil(static_cast<double>(mMessage.size()) / Packet::getPayloadMaxSize());
    }

    Transmission::Transmission(const Packet &packet)
        : mLogicAddress(packet.logicAddress),
          mMacAddress(packet.macAddress),
          mType(Type::INCOMING) {
        addIncomingPacket(packet);
    }


    std::optional<Packet> Transmission::getOutgoingPacket() {
        if (mType != Type::OUTGOING || mPacketsLeft == 0) return std::nullopt;
        return createNextPacket();
    }

    void Transmission::addIncomingPacket(const Packet &packet) {
        if (mType != Type::INCOMING) return;
        mMessage.insert(mMessage.end(), packet.payload.begin(), packet.payload.end());
    }

    Transmission::Type Transmission::getType() const {
        return mType;
    }

    Packet Transmission::createNextPacket() {
        const auto &payloadSize = Packet::getPayloadMaxSize();
        const std::span<const uint8_t> messageSpan = {mMessage.data(), mMessage.size()};
        const auto bytesToCopy = std::min(static_cast<size_t>(payloadSize), mMessage.size() - mMessageOffset);

        Packet packet;
        packet.logicAddress = mLogicAddress;
        packet.macAddress = mMacAddress;
        packet.packetsLeft = --mPacketsLeft;

        packet.payload.fill(Packet::getEndMarker());
        std::memcpy(packet.payload.data(), messageSpan.data() + mMessageOffset, bytesToCopy);
        mMessageOffset += bytesToCopy;

        packet.updateEndMarker();
        packet.updateChecksum();

        return packet;
    }

    std::array<uint8_t, 6> Transmission::stringMacToArray(const std::string_view macAddress) {
        static constexpr auto sBASE = 16;
        std::array<uint8_t, 6> macArray;


        for (int i = 0; i < 6; i++) {
            int offset = i * 3;

            auto [_, ec] = std::from_chars(
                macAddress.data() + offset,
                macAddress.data() + offset + 2,
                macArray[i],
                sBASE
            );

            if (ec != std::errc{}) {
                throw std::invalid_argument("Invalid MAC address format");
            }
        }

        return macArray;
    }

    // std::string Transmission::arrayMacToString(const std::array<uint8_t, 6> &macAddress) {
    //     char buffer[18];
    //     std::sprintf(buffer, "%02X:%02X:%02X:%02X:%02X:%02X",
    //                  macAddress[0], macAddress[1], macAddress[2], macAddress[3], macAddress[4], macAddress[5]);
    //     return std::string(buffer);
    // }
}
