#pragma once
#include "rf_driver/hc12_driver.h"
#include "rf_api.h"

#include <string>
#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <queue>
#include <span>
#include <vector>
#include <utility>

#include <boost/asio.hpp>


namespace ba = boost::asio;

namespace SmartHomeMediator {
    using namespace std::chrono_literals;

    struct Packet {
    private:
        static constexpr uint8_t msPAYLOAD_MAX_SIZE = 6;
        static constexpr uint16_t msCHECKSUM_MODULO = 256;
        static constexpr uint8_t msEND_MARKER = 0;
        static constexpr uint8_t msFILL_SYMBOL = 0;

    public:
        std::array<uint8_t, 6> macAddress;
        uint8_t logicAddress{};
        uint8_t packetsLeft{};
        std::array<uint8_t, msPAYLOAD_MAX_SIZE> payload;
        uint8_t checksum{};
        uint8_t endMarker{};

        static Packet from_bytes(std::span<const uint8_t> data);

        static Packet from_vector(const std::vector<uint8_t> &data);

        std::span<const uint8_t> as_bytes() const;

        std::vector<uint8_t> to_vector() const;

        bool isValid() const;

        bool isLastPacket() const;

        // std::vector<uint8_t> getPayload() const;

        static uint8_t getPayloadMaxSize();

        static uint8_t getEndMarker();

        static uint8_t getFillSymbol();

        void insertChecksum();

        void insertEndMarker();

    private:
        uint16_t static calculateChecksum(const Packet &packet);

        bool static verifyChecksum(const Packet &packet);
    } __attribute__((packed));


    class Session {
    public:
        enum class Type : uint8_t {
            FROM_CENTRAL_UNIT,
            FROM_MODULE
        };

        struct Metadata {
            Type sessionType = Type::FROM_CENTRAL_UNIT;
            uint8_t rfChannel{};
            uint8_t targetLogicAddress{};
            std::vector<RfApi::RfCommand> commands{};
        };

        Session(const Metadata &metadata, const std::shared_ptr<HC12Driver> &rfDriver, const std::shared_ptr<RfClient> &rfClient);

        ba::awaitable<std::string> execute();

        void addReceivedPacket(Packet packet);

    private:
        enum class State : uint8_t {
            NEXT_COMMAND,
            SEND_MESSAGE,
            AWAIT_RESPONSE,
            RESEND_LAST_MESSAGE,
            SEND_REPEAT_LAST_MESSAGE,
            SEND_PING,
            AWAIT_REPING,
            SEND_END_COMMAND,
            FINISHED
        };


        ba::awaitable<void> send(const std::vector<uint8_t> &message) const;

        ba::awaitable<std::vector<uint8_t> > receive();

        ba::awaitable<bool> changeChannel(uint8_t channel) const;

        static constexpr uint msMAX_REATTEMPTS = 3;
        static constexpr auto msSESSION_TIMEOUT = 10s;
        static constexpr auto msRECEIVE_MESSAGE_TIMEOUT = 2s;
        static constexpr auto msPOOLING_DELAY = 10ms;

        const Metadata mMetadata;
        std::shared_ptr<HC12Driver> mpRfDriver;
        std::shared_ptr<RfClient> mpRfClient;

        State mSessionCurrentState{};

        std::vector<uint8_t> mReceivedBuffer;
        std::mutex mReceiveMutex;
        std::atomic_bool mIsReceivedBufferReady{false};

        uint mFailedAttempts = 0;
    };
}
