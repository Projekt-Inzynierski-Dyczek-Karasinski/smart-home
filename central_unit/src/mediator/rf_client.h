#pragma once

#include "rf_driver/hc12_driver.h"
#include "async_logger.h"

#include <functional>
#include <memory>
#include <vector>
#include <c++/12/queue>

#include "session.h"

namespace ba = boost::asio;
namespace su = SmartHome::Utils;

namespace SmartHomeMediator {
    /**
     * @brief RF client with callback-based API for application layer.
     *
     * @details TODO !pr
     */
    class RfClient {
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

        void addToQueue(Session::Metadata &&metadata);

        // /**
        //  * //TODO !pr ?
        //  * @brief Send data via RF.
        //  *
        //  * @param data Binary data to send.
        //  * @param onComplete Callback with true on success, false on failure.
        //  */
        // void send(const std::vector<uint8_t> &data,
        //           const std::function<void(bool)> &onComplete) const;

        static uint8_t getDefaultChannel();

    private:
        /**
         * @brief Start receiving loop - echoes back received data.
         *
         *
         * @details Reads data from HC-12, logs it, then writes it back (echo).
         *          Runs in loop until RF client is destroyed.
         */
        void startReceiving();


        /**
         * @brief Internal coroutine for receive loop.
         */
        ba::awaitable<void> receiveLoop();

        static constexpr auto msAGGREGATE_RECEIVED_PACKET_INFO_TIMEOUT = 1s;

        static uint8_t msDefaultChannel;

        std::shared_ptr<HC12Driver> mpDriver;
        ba::io_context &mIoContext;
        std::shared_ptr<su::Logger> mpLogger;

        std::queue<Session::Metadata> mSessionQueue;
        std::mutex sessionQueueMutex;

        std::unique_ptr<Session> mCurrentSession;

        std::function<void(const std::string &message)> mMessageHandler;

        std::atomic_bool mIsReceiving = false;
        std::atomic_bool mIsShuttingDown = false;
    };
}
