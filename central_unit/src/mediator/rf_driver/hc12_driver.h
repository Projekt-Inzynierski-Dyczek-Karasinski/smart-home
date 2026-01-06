#pragma once

#include "rf_driver.h"
#include "uart_port.h"
#include "gpio_handler.h"
#include "async_logger.h"

#include <map>
#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <vector>


namespace ba = boost::asio;

namespace SmartHomeMediator {
    using namespace std::chrono_literals;
    namespace su = SmartHome::Utils;

    class HC12Driver final : public RfDriver {
    public:
        /**
         * @brief Internal option enumeration for type safety.
         */
        enum class Hc12Option {
            UNDEFINED = 0,
            CHANNEL, // AT+C (001-127)
            BAUDRATE, // AT+B (1200/2400/.../115200)
            FU_MODE, // AT+FU (1/2/3/4)
            POWER // AT+P (1-8)
        };

        /**
         * @brief Construct HC12 driver.
         *
         * @param ioContext Single-threaded IO context for RF operations.
         * @param logger Logger instance.
         * @param uartPortName UART device path.
         * @param uartBaudRate UART baud rate for RF mode.
         */
        HC12Driver(ba::io_context &ioContext,
                   const std::shared_ptr<su::Logger> &logger,
                   std::string_view uartPortName,
                   uint uartBaudRate);

        ~HC12Driver() override;

        // Prevent copying
        HC12Driver(const HC12Driver &) = delete;

        HC12Driver &operator=(const HC12Driver &) = delete;

        ba::awaitable<void> write(std::vector<uint8_t> data) override;

        ba::awaitable<std::vector<uint8_t> > read() override;


        ba::awaitable<bool> setOption(std::string option, std::string value) override;

        ba::awaitable<bool> setOption(Hc12Option option, const std::string& value);

        ba::awaitable<std::vector<uint8_t> > getOption(std::string option) override;

        ba::awaitable<std::string> getAllOptions() override;

        /**
         * @brief Get required delay between write operations.
         *
         * @details Returns 2000ms for FU2/FU4 modes, 40ms for FU1/FU3.
         * @return Delay duration.
         */
        std::chrono::milliseconds getRequiredWriteDelay() const;

    private:
        /**
         * @brief Enter AT command mode.
         *
         * @details Sets SET pin LOW, waits 40ms, switches to 9600 baud.
         */
        ba::awaitable<void> enterConfigMode();

        /**
         * @brief Exit AT command mode.
         *
         * @details Sets SET pin HIGH, waits 80ms, restores original baud rate.
         */
        ba::awaitable<void> exitConfigMode();

        /**
         * @brief Send AT command and wait for response.
         *
         * @param command AT command.
         * @param timeout Max wait time for response.
         * @return Response string without CRLF.
         */
        ba::awaitable<std::vector<uint8_t> > sendAtCommand(std::vector<uint8_t> command,
                                                           std::chrono::milliseconds timeout = 400ms) const;

        /**
         * @brief Convert string to internal option enum.
         */
        static Hc12Option stringToOption(std::string_view option);

        /**
         * @brief Convert internal option enum to string.
         */
        static std::string optionToString(Hc12Option option);

        /**
         * @brief Build AT set command for given option.
         *
         * @param option Option to set.
         * @param value Value to set.
         * @return AT command.
         */
        static std::vector<uint8_t> buildSetCommand(Hc12Option option, std::string_view value);

        /**
         * @brief Build AT query command for given option.
         *
         * @param option Option to query.
         * @return AT command.
         */
        static std::vector<uint8_t> buildGetCommand(Hc12Option option);

        /**
         * @brief Parse single-line AT response.
         *
         * @param response Response from HC-12.
         * @return Extracted value.
         */
        static std::vector<uint8_t> parseResponse(const std::vector<uint8_t> &response);

        /**
         * @brief Map power level (1-8) to dBm string.
         */
        static std::string_view powerLevelToDbm(std::string_view level);

        /**
         * @brief Map dBm string to power level (1-8).
         */
        static std::vector<uint8_t> dbmToPowerLevel(std::string_view dbm);

        // Hardware components
        std::unique_ptr<UartPort> mpUart;
        std::unique_ptr<GpioHandler> mpSetPin;

        // Mediator components
        std::shared_ptr<su::Logger> mpLogger;
        ba::io_context &mIoContext;

        // Configuration
        std::map<std::string, std::string> mOptionsCache;
        uint mOriginalBaudRate;
        bool mIsInConfigMode = false;
        std::chrono::steady_clock::time_point mLastWriteTime;

        static constexpr std::array<std::string_view, 8> mDbmArray = {"1", "2", "5", "8", "11", "14", "17", "20"};

        static constexpr auto msCONFIG_TIMEOUT = 400ms;
        static constexpr std::string_view mUART_DEVICE_PATH = "/dev/gpiochip0";
        static constexpr int ms_SET_PIN = 18; ///< GPIO pin number for SET control (BCM numbering).
    };
}
