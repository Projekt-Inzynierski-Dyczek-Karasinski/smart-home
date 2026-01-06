#pragma once
#include "rf_driver/hc12_driver.h"
#include "rf_types.h"

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
    class RfClient;
    using namespace std::chrono_literals;

    class Session {
    public:
        Session(RfTypes::SessionMetadata metadata,
            const std::shared_ptr<HC12Driver> &pRfDriver,
            const std::shared_ptr<RfClient> &pRfClient,
            const std::shared_ptr<su::Logger> &pLogger);

        ba::awaitable<std::string> execute();

        void addReceivedPacket(RfTypes::Packet packet);

    private:
        enum class State : uint8_t {
            NEXT_COMMAND,
            SEND_MESSAGE,
            AWAIT_RESPONSE,
            RESEND_LAST_MESSAGE,
            SEND_REPEAT_LAST_MESSAGE,
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

        const RfTypes::SessionMetadata mMetadata;
        std::shared_ptr<HC12Driver> mpRfDriver;
        std::shared_ptr<RfClient> mpRfClient;
        std::shared_ptr<su::Logger> mpLogger;

        State mSessionCurrentState{};

        std::vector<uint8_t> mReceivedBuffer;
        std::mutex mReceiveMutex;
        std::atomic_bool mIsReceivedBufferReady{false};

        uint mFailedAttempts = 0;
    };
}
