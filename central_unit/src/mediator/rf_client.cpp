#include "rf_client.h"
#include "mediator.h"

namespace SmartHomeMediator {
    RfClient::RfClient(ba::io_context &ioContext,
                       const std::shared_ptr<su::Logger> &logger,
                       const std::string_view uartPortName,
                       const uint uartBaudRate)
        : mIoContext(ioContext), mpLogger(logger) {
        try {
            mpDriver = std::make_shared<HC12Driver>(
                mIoContext,
                mpLogger,
                uartPortName,
                uartBaudRate
            );

            mpLogger->info("[RF_CLIENT] RF client created");
        } catch (const std::exception &e) {
            mpLogger->errorf("[RF_CLIENT] Failed to create RF client: %s", e.what());
            throw;
        }

        mDefaultChannel = 1; //TODO !pr read from conf
        mDefaultMac = {1, 1, 1, 1, 1, 1}; //TODO !pr read from conf
    }

    RfClient::~RfClient() {
        mIsShuttingDown = true;
        mpLogger->info("[RF_CLIENT] RF client destroyed");
    }

    void RfClient::initialize(const std::function<void(bool)> &onComplete) const {
        mpLogger->info("[RF_CLIENT] Initializing - setting channel to default");

        ba::co_spawn(mIoContext, [this, onComplete]() -> ba::awaitable<void> {
            bool success = false;

            // Set channel to default
            try {
                success = co_await mpDriver->setOption(HC12Driver::Hc12Option::CHANNEL,
                                                       std::to_string(mDefaultChannel));
            } catch (const std::exception &e) {
                mpLogger->errorf("[RF_CLIENT] Initialization error: %s", e.what());
                onComplete(false);
                co_return;
            }

            if (success) {
                mpLogger->info("[RF_CLIENT] Initialization complete - channel set to default");
            } else {
                mpLogger->error("[RF_CLIENT] Initialization failed - could not set channel");
            }

            onComplete(success);
            co_return;
        }, ba::detached);
    }

    ba::awaitable<void> RfClient::run(std::function<void(const std::string &message)> handleMessage) {
        mpLogger->debug("[RF_CLIENT] [RUN] called");
        auto &mediator = Mediator::Instance();
        mMessageHandler = std::move(handleMessage);

        startReceiving();
        startSending();
        boost::asio::steady_timer timer(co_await boost::asio::this_coro::executor);

        mpLogger->info("[RF_CLIENT] Started RfClient");
        while (!mIsShuttingDown) {
            if (mSessionQueue.empty()) {
                // Wait before next pooling
                timer.expires_after(msPOOLING_DELAY);
                co_await timer.async_wait(boost::asio::use_awaitable);
                continue;
            }

            RfTypes::SessionMetadata meta;
            // Get next session metadata
            {
                std::scoped_lock lock(mSessionQueueMutex);
                meta = mSessionQueue.front();
                mSessionQueue.pop();
            }

            mCurrentSession = std::make_unique<Session>(meta, mpDriver, shared_from_this(), mpLogger);

            auto result = co_await mCurrentSession->execute();

            mCurrentSession.reset();

            mediator.getIoContext().post([this, result] {
                mMessageHandler(result);

            });
        }
        mpLogger->info("[RF_CLIENT] Stopping RfClient");
        mIsReceiving = false;
        mIsSending = false;
    }

    void RfClient::addSession(RfTypes::SessionMetadata &&metadata) {
        mpLogger->debug("[RF_CLIENT] Adding session");
        std::scoped_lock lock(mSessionQueueMutex);
        mSessionQueue.push(std::move(metadata));
    }

    void RfClient::addMessageToSend(std::vector<uint8_t> &&message) {
        std::scoped_lock lock(mSessionQueueMutex);
        mSendQueue.push(std::move(message));
    }

    bool RfClient::isSendQueueEmpty(){
        std::scoped_lock lock(mSendQueueMutex);
        return mSendQueue.empty();
    }

    uint8_t RfClient::getDefaultChannel() {
        return mDefaultChannel;
    }

    std::array<uint8_t, 6> RfClient::getDefaultMacAddress() {
        return mDefaultMac;
    }

    void RfClient::startReceiving() {
        bool expected = false;
        constexpr bool desired = true;
        if (!mIsReceiving.compare_exchange_strong(expected, desired)) {
            mpLogger->warning("[RF_CLIENT] Already receiving");
            return;
        }

        mpLogger->info("[RF_CLIENT] Starting receive loop");
        ba::co_spawn(mIoContext, receiveLoop(), ba::detached);
    }

    void RfClient::startSending() {
        bool expected = false;
        constexpr bool desired = true;
        if (!mIsSending.compare_exchange_strong(expected, desired)) {
            mpLogger->warning("[RF_CLIENT] Already sending");
            return;
        }

        mpLogger->info("[RF_CLIENT] Starting send loop");
        ba::co_spawn(mIoContext, sendLoop(), ba::detached);
    }

    ba::awaitable<void> RfClient::receiveLoop() {
        auto loggerAggregateTimer = ba::steady_timer(co_await ba::this_coro::executor);
        std::atomic<uint> packetReceived = 0;
        std::atomic<uint> packetInvalid = 0;
        std::atomic<uint> packetEmpty = 0;

        while (mIsReceiving) {
            std::vector<uint8_t> data;

            try {
                data = co_await mpDriver->read();
                ++packetReceived;
            } catch (const std::exception &e) {
                mpLogger->debugf("[RF_CLIENT] Receive error: %s", e.what());
            }

            if (data.empty()) {
                mpLogger->debug("[RF_CLIENT] Receive data is empty");
                ++packetEmpty;
                continue;
            }

            RfTypes::Packet packet;
            try {
                packet = RfTypes::Packet::from_vector(data);
            }
            catch (const std::exception &e) {
                mpLogger->debugf("[RF_CLIENT] invalid packet: %s", e.what());
                ++packetInvalid;
                continue;
            }

            if (!packet.isValid()) {
                mpLogger->debug("[RF_CLIENT] Received invalid packet");
                ++packetInvalid;
                continue;
            }

            if (mCurrentSession) {
                mCurrentSession->addReceivedPacket(packet);
            } else {
                mpLogger->debug("[RF_CLIENT] Creating new session"); //TODO !pr test
                auto meta = RfTypes::SessionMetadata{
                    .sessionType = RfTypes::SessionType::FROM_MODULE,
                    .rfChannel = mDefaultChannel,
                    .targetLogicAddress = packet.logicAddress
                };

                std::queue<RfTypes::SessionMetadata> tmpQueue;
                tmpQueue.emplace(std::move(meta));

                // Emplace notification in first place of session queue
                {
                    std::scoped_lock lock(mSessionQueueMutex);

                    while (!mSessionQueue.empty()) {
                        tmpQueue.push(mSessionQueue.front());
                        mSessionQueue.pop();
                    }
                    mSessionQueue = std::move(tmpQueue);
                }
            };

            // Aggregate packet info for logging
            loggerAggregateTimer.cancel();
            loggerAggregateTimer.expires_after(msAGGREGATE_RECEIVED_PACKET_INFO_TIMEOUT);
            loggerAggregateTimer.async_wait(
                [this, &packetReceived, &packetInvalid, &packetEmpty](const bs::error_code &ec) {
                    if (!ec && packetReceived > 0) {
                        mpLogger->infof(
                            "[RF_CLIENT] Received %d packets, %2.2f%% packet loss (%d invalid, %d dropped)",
                            packetReceived.load(),
                            (packetInvalid + packetEmpty) / static_cast<double>(packetReceived),
                            packetInvalid.load(),
                            packetEmpty.load());
                        packetReceived = 0;
                        packetInvalid = 0;
                        packetEmpty = 0;
                    }
                });
        }

        mpLogger->info("[RF_CLIENT] Receive loop stopped");
        co_return;
    }

    ba::awaitable<void> RfClient::sendLoop() {
        boost::asio::steady_timer timer(co_await boost::asio::this_coro::executor);

        while (mIsSending) {
            if (mSendQueue.empty()) {
                // Wait before next pooling
                timer.expires_after(msPOOLING_DELAY);
                co_await timer.async_wait(ba::use_awaitable);
                continue;
            }

            std::vector<uint8_t> data;
            // Get data to send
            {
                std::scoped_lock lock(mSendQueueMutex);
                data = std::move(mSendQueue.front());
                mSendQueue.pop();
            }

            co_await mpDriver->write(data);
        }
        mpLogger->info("[RF_CLIENT] Send loop stopped");
        co_return;
    }
}
