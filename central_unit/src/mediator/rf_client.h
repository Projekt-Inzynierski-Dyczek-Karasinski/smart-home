#pragma once

#include "rf_types.h"
#include "session.h"
#include "rf_driver/hc12_driver.h"
#include "async_logger.h"

#include <functional>
#include <memory>
#include <vector>
#include <queue>


namespace ba = boost::asio;
namespace su = SmartHome::Utils;

namespace SmartHomeMediator {
    /**
     * @brief RF client with callback-based API for application layer.
     *
     * @details Manages RF driver lifecycle, session queue, and bidirectional message flows.
     *          Coordinates receive/send loops with session execution.
     *          Routes incoming notifications and outgoing commands through callback interface.
     */
    class RfClient : public std::enable_shared_from_this<RfClient> {
    public:
        /**
         * @brief Config with basic RfClient configuration.
         */
        struct Config {
            HC12Driver::Config rfDriverConfig{};
            std::array<uint8_t, 6> uniqueNetworkId = {1,1,1,1,1,1}; //Default 1:1:1:1:1:1 for testing
            uint8_t mainRfChannel = 1;
        };


        /**
         * @brief Construct RF client.
         *
         * @param ioContext Single-threaded IO context for RF operations.
         * @param logger Logger instance.
         * @param config Struct with RfClient config.
         */
        RfClient(ba::io_context &ioContext,
                 const std::shared_ptr<su::Logger> &logger,
                 const Config &config);

        ~RfClient();

        // Prevent copying
        RfClient(const RfClient &) = delete;

        RfClient &operator=(const RfClient &) = delete;

        /**
         * @brief Initialize RF client - sets channel to 1.
         *
         * @param onComplete Callback with true on success, false on failure.
         */
        void initialize(const std::function<void(bool)> &onComplete) const;

        /**
         * @brief Main execution loop for RF client.
         *
         * @details Processes session queue, manages current session lifecycle,
         *          and routes results through message handler callback.
         *          Runs until shutdown requested.
         *
         * @param handleMessage Callback for outgoing API messages.
         *
         * @return Awaitable that completes on shutdown.
         */
        ba::awaitable<void> run(std::function<void(const std::string &message)> handleMessage);

        /**
         * @brief Add session to execution queue.
         *
         * @param metadata Session configuration with commands to execute.
         */
        void addSession(RfTypes::SessionMetadata &&metadata);

        /**
         * @brief Add message to send queue.
         *
         * @param message Binary message for transmission.
         */
        void addMessageToSend(std::vector<uint8_t> &&message);

        /**
         * @brief Check if send queue is empty.
         *
         * @return true if no messages pending, false otherwise.
         */
        bool isSendQueueEmpty();

        /**
         * @brief Get configured main RF channel.
         *
         * @return Main RF channel number.
         */
        uint8_t getRfMainChannel() const;


        /**
         * @brief Get unique network identifier (MAC address).
         *
         * @return 6-byte MAC address array.
         */
        std::array<uint8_t, 6> getUniqueNetworkId() const;

    private:
        /**
         * @brief Start background receive loop.
         *
         * @details Spawns coroutine that continuously reads RF packets, validates them,
         *          and routes to current session or creates new notification session.
         */
        void startReceiving();

        /**
         * @brief Start background send loop.
         *
         * @details Spawns coroutine that processes send queue.
         */
        void startSending();


        /**
         * @brief Continuous packet receive coroutine.
         *
         * @details Reads packets from driver, validates, aggregates statistics,
         *          and routes to current session or creates notification sessions.
         *
         * @return Awaitable that runs until mIsReceiving flag cleared.
         */
        ba::awaitable<void> receiveLoop();

        /**
         * @brief Continuous message send coroutine.
         *
         * @details Processes send queue and writes messages via driver.
         *
         * @return Awaitable that runs until mIsSending flag cleared.
         */
        ba::awaitable<void> sendLoop();

        /// Timeout for aggregating packet statistics in logs
        static constexpr auto msAGGREGATE_RECEIVED_PACKET_INFO_TIMEOUT = 1s;
        /// Delay between queue polling iterations
        static constexpr auto msPOOLING_DELAY = 1ms;

        uint8_t mRfMainChannel;
        std::array<uint8_t,6> mUniqueNetworkId{}; // Defaults to device MAC address

        std::shared_ptr<HC12Driver> mpDriver;
        ba::io_context &mIoContext;
        std::shared_ptr<su::Logger> mpLogger;

        std::queue<RfTypes::SessionMetadata> mSessionQueue{};
        std::mutex mSessionQueueMutex;
        std::unique_ptr<Session> mCurrentSession{};

        std::mutex mSendQueueMutex;
        std::queue<std::vector<uint8_t>> mSendQueue{};

        std::function<void(const std::string &message)> mMessageHandler;

        std::atomic_bool mIsReceiving = false;
        std::atomic_bool mIsSending = false;
        std::atomic_bool mIsShuttingDown = false;
    };
}
