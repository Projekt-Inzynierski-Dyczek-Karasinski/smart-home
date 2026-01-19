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
#include <set>


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
            FU_MODE, // AT+FU (1-4)
            POWER, // AT+P (1-8)
            DEFAULT // Reset to default
        };

        struct Config {
            std::string uartPortPath = "/dev/serial0";
            std::string chipPath = "/dev/gpiochip0";
            uint uartBaudrate = 9600;
            uint setPin = 18;
        };

        /**
         * @brief Construct HC12 driver.
         *
         * @param ioContext Single-threaded IO context for RF operations.
         * @param logger Logger instance.
         * @param config Struct with HC-12 basic config.
         */
        HC12Driver(ba::io_context &ioContext,
                   const std::shared_ptr<su::Logger> &logger,
                   Config config);

        ~HC12Driver() override;

        // Prevent copying
        HC12Driver(const HC12Driver &) = delete;

        HC12Driver &operator=(const HC12Driver &) = delete;

        ba::awaitable<void> write(std::vector<uint8_t> data) override;

        ba::awaitable<std::vector<uint8_t> > read() override;

        ba::awaitable<bool> setOption(std::string option, std::string value) override;

        ba::awaitable<std::vector<uint8_t> > getOption(std::string option) override;

        ba::awaitable<std::string> getAllOptions() override;

        /**
         * @brief Get required delay between write operations.
         *
         * @details Returns 2000ms for FU2/FU4 modes, 40ms for FU1/FU3.
         * @return Delay duration.
         */
        std::chrono::milliseconds getRequiredWriteDelay() override;

        bool isMultiChannel() override;

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

        static bool isChannelValid(int channel);

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

        // Power level to db array, with value from HC-12 user spreadsheet.
        static constexpr std::array<std::string_view, 8> msDBM_ARRAY = {"1", "2", "5", "8", "11", "14", "17", "20"};

        static inline const std::set msVALID_BAUDRATE = {1200, 2400, 4800, 9600, 19200, 38400,  57600, 115200};
        static constexpr auto msMIN_CHANNEL = 1;
        static constexpr auto msMAX_CHANNEL = 127;
        static inline const std::set msVALID_FU_MODE = {1,2,3,4};
        static inline const std::set ms_VALID_POWER = {1,2,3,4,5,6,7,8};

        // Default for FU1, FU3, or unknown. 80ms is the most reliable value (lower values caused data loss)
        static constexpr auto msLOW_WRITE_DELAY = 80ms;
        // Default for FU2, FU4. 2000ms is value recommended by HC-12 user spreadsheet.
        static constexpr auto msHIGH_WRITE_DELAY = 2000ms;

        static constexpr auto msCONFIG_TIMEOUT = 400ms;

        static constexpr bool msIsMultiChannel = true;
    };
}
