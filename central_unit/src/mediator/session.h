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
            SEND_END_COMMAND,
            SEND_ACK_COMMAND,
            SEND_NEG_COMMAND,
            SEND_MESSAGE,
            SEND_REPEAT_LAST_MESSAGE,
            RESEND_LAST_MESSAGE,
            AWAIT_RESPONSE,
            AWAIT_NOTIFICATION,
            FINISHED
        };

        struct ExecutionContext {
            State state;
            State previousState;

            std::unique_ptr<RfTypes::RfCommand> pCommandResponse;
            std::unique_ptr<RfTypes::RfCommand>  pCurrentCommand;
            RfTypes::RfCommand lowLevelCommand;
            std::vector<uint8_t> receivedMessage;
            std::vector<uint8_t> lastSendMessage;

            std::vector<std::string> resultsVector;
            SmartHome::API::ApiResponse response;
            SmartHome::API::ApiError error;

            size_t commandsVectorIndex = 0;
            uint retries = 0;
            bool isInitializedFromModule;

            ba::steady_timer* delayTimer;

            std::function<void(State)> changeState;

            ExecutionContext(const bool initFromModule, ba::steady_timer* timer)
                : isInitializedFromModule(initFromModule)
                , delayTimer(timer) {

                previousState = State::NEXT_COMMAND;
                state = isInitializedFromModule ? State::AWAIT_NOTIFICATION : State::NEXT_COMMAND;

                changeState = [this](const State newState) {
                    previousState = state;
                    state = newState;
                };
            }
        };

        ba::awaitable<std::string> handleMediatorConfigSession() const;

        ba::awaitable<void> processState(ExecutionContext& ctx);

        ba::awaitable<void> send(const std::vector<uint8_t> &message) const;

        ba::awaitable<std::vector<uint8_t> > receive();

        ba::awaitable<bool> changeChannel(uint8_t channel) const;

        ba::awaitable<bool> acquireConnection();

        static constexpr uint msMAX_REATTEMPTS = 3;
        static constexpr auto msSESSION_TIMEOUT = 10s;
        static constexpr auto msRECEIVE_MESSAGE_TIMEOUT = 2s;
        static constexpr auto msPOOLING_DELAY = 10ms;

        RfTypes::SessionMetadata mMetadata;
        std::shared_ptr<HC12Driver> mpRfDriver;
        std::shared_ptr<RfClient> mpRfClient;
        std::shared_ptr<su::Logger> mpLogger;

        std::vector<uint8_t> mReceivedBuffer;
        std::mutex mReceiveMutex;
        std::atomic_bool mIsReceivedBufferReady{false};

    };
}
