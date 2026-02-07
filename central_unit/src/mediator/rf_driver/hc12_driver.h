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

// TODO Setting FU_MODE can change HC-12 baudrate, implement automatic baudrate change in UART port to handle this case

namespace ba = boost::asio;

namespace SmartHomeMediator {
    using namespace std::chrono_literals;
    namespace su = SmartHome::Utils;

    class HC12Driver final : public RfDriver {
    public:
        /**
         * @brief Internal option enumeration.
         */
        enum class Hc12Option {
            UNDEFINED = 0,
            CHANNEL, // AT+C (001-127)
            BAUDRATE, // AT+B (1200/2400/.../115200)
            FU_MODE, // AT+FU (1-4)
            POWER, // AT+P (1-8)
            DEFAULT // Reset to default, requires value "all_options"
        };

        struct Config {
            std::string uartPortPath = "/dev/serial0";
            std::string chipPath = "/dev/gpiochip0";
            uint uartBaudrate = 9600;
            uint setPin = 18;
        };

        /**
         * @brief Construct HC-12 driver.
         *
         * @param ioContext Single-threaded IO context for RF operations.
         * @param logger Logger instance.
         * @param config Struct with HC-12 basic config.
         */
        HC12Driver(ba::io_context &ioContext,
                   const std::shared_ptr<su::Logger> &logger,
                   const Config &config);

        ~HC12Driver() override;

        HC12Driver(const HC12Driver &) = delete;

        HC12Driver &operator=(const HC12Driver &) = delete;

        /**
         * @brief Write data to HC-12 in transparent mode.
         *
         * @details Waits before write for at least \c getRequiredWriteDelay() of time since last write.
         *
         * @param data Binary data to transmit.
         *
         * @return Awaitable that completes when write finishes.
         *
         * @throws std::exception propagates exceptions from UART port operations.
         */
        ba::awaitable<void> write(std::vector<uint8_t> data) override;

        /**
         * @brief Read data from HC-12 in transparent mode.
         *
         * @return Awaitable with received data string.
         *
         * @throws std::exception propagates exceptions from UART port operations.
         */
        ba::awaitable<std::vector<uint8_t> > read() override;

        /**
         * @brief Set HC-12 AT option.
         *
         * @details Validates options and values before setting and results after.
         *          On successful operation updates cache with option:new_value.
         *
         * @param option Option name.
         * @param value Option value as string.
         *
         * @return Awaitable with true on success, false on failure.
         *
         * @throws std::invalid_argument on invalid option or value.
         * @throws std::exception propagates exceptions from UART port operations.
         * @throws std::runtime_error on exit/enter config mode failure.
         */
        ba::awaitable<bool> setOption(std::string option, std::string value) override;

        /**
         * @brief Get HC-12 configuration option.
         *
         * @brief Tries to query cached values first, on successful HC-12 query updates cache.
         *
         * @param option Option name.
         *
         * @return Awaitable with option value, or empty string on error.
         *
         * @throws std::invalid_argument on invalid option.
         * @throws std::exception propagates exceptions from UART port operations.
         * @throws std::runtime_error on exit/enter config mode failure.
         */
        ba::awaitable<std::vector<uint8_t> > getOption(std::string option) override;

        /**
         * @brief Get all HC-12 configuration options.
         *
         * @return Awaitable with formatted string "option:value,option:value,...".
         *
         * @note If failed to query a value "error_value" will be inserted instead.
         */
        ba::awaitable<std::string> getAllOptions() override;

        /**
         * @brief Get required delay between write operations.
         *
         * @details Returns 2000ms for FU2/FU4 modes, 40ms for FU1/FU3.
         *
         * @return Delay duration.
         */
        std::chrono::milliseconds getRequiredWriteDelay() override;

        /**
         * @brief Getter for msIsMultiChannel.
         *
         * @return msIsMultiChannel value.
         */
        bool isMultiChannel() override;

    private:
        /**
         * @brief Enter AT command mode.
         *
         * @details Sets SET pin LOW, waits 40ms.
         *
         * @throws std::runtime_error on failure.
         */
        ba::awaitable<void> enterConfigMode();

        /**
         * @brief Exit AT command mode.
         *
         * @details Sets SET pin HIGH, waits 80ms.
         *
         * @throws std::runtime_error on failure.
         */
        ba::awaitable<void> exitConfigMode();

        /**
         * @brief Send AT command and wait for response.
         *
         * @param command AT command.
         * @param timeout Max wait time for response
         * .
         * @return Response string without CRLF.
         *
         * @throws std::exception propagates exceptions from UART port operations.
         */
        [[nodiscard]] ba::awaitable<std::vector<uint8_t> > sendAtCommand(std::vector<uint8_t> command,
                                                                         std::chrono::milliseconds timeout = 400ms)
        const;

        /**
         * @brief Convert string to internal option enum.
         *
         * @return Hc12Option enum value corresponding to string value, or Hc12Option::UNDEFINED
         */
        static Hc12Option stringToOption(std::string_view option);

        /**
         * @brief Convert internal option enum to string.
         *
         * @return String with corresponding to option value, or "undefined".
         */
        static std::string optionToString(Hc12Option option);

        /**
         * @brief Build AT set command for given option.
         *
         * @param option Option to set.
         * @param value Value to set.
         *
         * @return Vector with AT command, empty if invalid.
         */
        static std::vector<uint8_t> buildSetCommand(Hc12Option option, std::string_view value);

        /**
         * @brief Build AT query command for given option.
         *
         * @param option Option to query.
         *
         * @return Vector with AT command.
         */
        static std::vector<uint8_t> buildGetCommand(Hc12Option option);

        /**
         * @brief Parse single-line AT response.
         *
         * @param response Response from HC-12.
         *
         * @return Vector with extracted value, defaults to "AT".
         */
        static std::vector<uint8_t> parseResponse(const std::vector<uint8_t> &response);

        /**
         * @brief Map dBm string to power level (1-8).
         *
         * @return String with power level corresponding to DBm, empty if invalid.
         */
        static std::string dbmToPowerLevel(std::string_view dbm);

        /**
         * @brief Helper function for validating if channel is in [msMIN_CHANNEL, msMAX_CHANNEL] range.
         *
         * @param channel RF channel number.
         *
         * @return true if channel is valid.
         */
        static bool isChannelValid(int channel);

        // Hardware components
        std::unique_ptr<UartPort> mpUart;
        std::unique_ptr<GpioHandler> mpSetPin;

        // Mediator components
        std::shared_ptr<su::Logger> mpLogger;
        ba::io_context &mIoContext;

        // Configuration
        uint mOriginalBaudRate;
        std::chrono::steady_clock::time_point mLastWriteTime;
        std::map<std::string, std::string> mOptionsCache;
        std::atomic_bool mIsInConfigMode = false;

        static constexpr bool msIsMultiChannel = true;

        static constexpr auto msCONFIG_TIMEOUT = 400ms;

        //Options valid values
        /// Power level to db array, with value from HC-12 user spreadsheet.
        static constexpr std::array<std::string_view, 8> msDBM_ARRAY = {"1", "2", "5", "8", "11", "14", "17", "20"};
        static inline const std::set msVALID_BAUDRATE = {1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200};
        static constexpr auto msMIN_CHANNEL = 1;
        static constexpr auto msMAX_CHANNEL = 127;
        static inline const std::set msVALID_FU_MODE = {1, 2, 3, 4};
        static inline const std::set ms_VALID_POWER = {1, 2, 3, 4, 5, 6, 7, 8};

        // Default for FU1, FU3, or unknown. 80ms is the most reliable value (lower values caused data loss)
        static constexpr auto msLOW_WRITE_DELAY = 80ms;
        // Default for FU2, FU4. 2000ms is value recommended by HC-12 user spreadsheet.
        static constexpr auto msHIGH_WRITE_DELAY = 2000ms;
    };
}
