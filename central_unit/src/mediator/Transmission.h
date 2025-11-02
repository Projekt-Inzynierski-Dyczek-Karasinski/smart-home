#pragma once

#include <string>
#include <array>
#include <cstdint>
#include <optional>
#include <queue>
#include <span>
#include <vector>

namespace SmartHomeMediator {
    struct Packet {
    private:
        static constexpr uint8_t ms_PAYLOAD_MAX_SIZE = 6;
        static constexpr uint16_t ms_CHECKSUM_MODULO = 256;
        static constexpr uint8_t ms_END_MARKER = 0;

    public:
        uint8_t logicAddress;
        std::array<uint8_t, 6> macAddress;
        uint8_t packetsLeft;
        std::array<uint8_t, ms_PAYLOAD_MAX_SIZE> payload;
        uint8_t checksum;
        uint8_t endMarker;

        std::span<const uint8_t> as_bytes() const;

        Packet from_bytes(std::span<const uint8_t> data) const;

        std::vector<uint8_t> to_vector() const;

        Packet from_vector(const std::vector<uint8_t> &data) const;

        bool isValid() const;

        bool isLastPacket() const;

        // std::vector<uint8_t> getPayload() const;

        static uint8_t getPayloadMaxSize();

        static uint16_t getEndMarker();

        void updateChecksum();

        void updateEndMarker();

    private:
        uint16_t static calculateChecksum(const Packet &packet);

        bool static verifyChecksum(const Packet &packet);
    } __attribute__((packed));


    class Transmission {
        //TODO
        //  - divide RfClient message into packets and send
        //  - receive packets combining them and pass to RfClient,
        //  - RF transmission logic (retry, ack etc...)
        //  - RfClient has transmission map (separate for i/o?)
        //  - Transmission has static method to verify and id packet before adding it to instance


    public:
        enum class Type {
            UNDEFINED,
            OUTGOING,
            INCOMING
        };

        Transmission(uint8_t logicAddress, std::string_view macAddress, std::string_view message);

        explicit Transmission(const Packet &packet);

        std::optional<Packet> getOutgoingPacket();

        void addIncomingPacket(const Packet &packet);

        Type getType() const;

    private:
        Packet createNextPacket();

        static inline std::array<uint8_t, 6> stringMacToArray(std::string_view macAddress);

        // static inline std::string arrayMacToString(const std::array<uint8_t, 6> &macAddress);

        const uint8_t mLogicAddress;
        const std::array<uint8_t, 6> mMacAddress;
        const Type mType = Type::UNDEFINED;

        std::vector<uint8_t> mMessage;
        size_t mMessageOffset = 0;
        size_t mPacketsLeft;
    };
}
