#pragma once

#include "rf_driver/hc12_driver.h"
#include "async_logger.h"

#include <functional>
#include <memory>
#include <vector>

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
        RfClient(ba::io_context& ioContext,
                 const std::shared_ptr<su::Logger>& logger,
                 std::string_view uartPortName,
                 uint uartBaudRate);

        ~RfClient();

        // Prevent copying
        RfClient(const RfClient&) = delete;
        RfClient& operator=(const RfClient&) = delete;

        /**
         * @brief Initialize RF client - sets channel to 1.
         *
         * @param onComplete Callback with true on success, false on failure.
         */
        void initialize(const std::function<void(bool)> &onComplete) const;

        /**
         * @brief Start receiving loop - echoes back received data.
         *
         * @param onDataReceived Callback invoked with received data.
         *
         * @details Reads data from HC-12, logs it, then writes it back (echo).
         *          Runs in loop until RF client is destroyed.
         */
        void startReceiving(const std::function<void(const std::vector<uint8_t> &)> &onDataReceived);

        /**
         * @brief Send data via RF.
         *
         * @param data Binary data to send.
         * @param onComplete Callback with true on success, false on failure.
         */
        void send(const std::vector<uint8_t> &data,
                  const std::function<void(bool)> &onComplete) const;

    private:
        /**
         * @brief Internal coroutine for receive loop.
         */
        ba::awaitable<void> receiveLoop(std::function<void(const std::vector<uint8_t> &)> onDataReceived) const;

        std::unique_ptr<HC12Driver> mpDriver;
        ba::io_context& mIoContext;
        std::shared_ptr<su::Logger> mpLogger;

        bool mIsReceiving = false;
    };

}