#pragma once

#include "rf_driver/hc12_driver.h"
#include "async_logger.h"
#include "session.h"

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
     * @details TODO !pr
     */
    class RfClient : public std::enable_shared_from_this<RfClient> {
    public:
        /**
         * @brief Construct RF client.
         *
         * @param ioContext Single-threaded IO context for RF operations.
         * @param logger Logger instance.
         * @param uartPortName UART device path.
         * @param uartBaudRate UART baud rate for RF mode.
         */
        RfClient(ba::io_context &ioContext,
                 const std::shared_ptr<su::Logger> &logger,
                 std::string_view uartPortName,
                 uint uartBaudRate);

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

        ba::awaitable<void> run(std::function<void(const std::string &message)> handleMessage);

        void addSession(Session::Metadata &&metadata);

        void addMessageToSend(std::vector<uint8_t> &&message);

        static uint8_t getDefaultChannel();

        static std::array<uint8_t, 6> getDefaultMacAddress();

    private:
        /**
         * @brief Start receiving loop - echoes back received data.
         *
         *
         * @details Reads data from HC-12, logs it, then writes it back (echo).
         *          Runs in loop until RF client is destroyed.
         */
        void startReceiving();

        void startSending();


        /**
         * @brief Internal coroutine for receive loop.
         */
        ba::awaitable<void> receiveLoop();

        ba::awaitable<void> sendLoop();

        static constexpr auto msAGGREGATE_RECEIVED_PACKET_INFO_TIMEOUT = 1s;
        static constexpr auto msPOOLING_DELAY = 10ms;

        static uint8_t msDefaultChannel;
        static std::array<uint8_t,6> msDefaultMac;

        std::shared_ptr<HC12Driver> mpDriver;
        ba::io_context &mIoContext;
        std::shared_ptr<su::Logger> mpLogger;

        std::queue<Session::Metadata> mSessionQueue{};
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
