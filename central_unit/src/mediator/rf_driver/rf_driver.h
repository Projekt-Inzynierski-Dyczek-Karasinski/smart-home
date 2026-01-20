#pragma once

#include "async_logger.h"

#include <string>
#include <string_view>
#include <vector>
#include <memory>

#include <boost/asio/awaitable.hpp>

namespace su = SmartHome::Utils;
namespace ba = boost::asio;

namespace SmartHomeMediator {
    /**
     * @brief Generic RF transceiver interface with awaitable operations.
     *
     * @details Provides abstract interface for RF transceiver with coroutine-based
     *          async operations. Implementations handle specific hardware (HC-12, etc.).
     */
    class RfDriver {
    public:
        virtual ~RfDriver() = default;

        /**
         * @brief Write data to RF transceiver in transparent mode.
         *
         * @param data Binary data to transmit.
         *
         * @return Awaitable that completes when write finishes.
         */
        virtual ba::awaitable<void> write(std::vector<uint8_t> data) = 0;

        /**
         * @brief Read data from RF transceiver in transparent mode.
         *
         * @return Awaitable with received data string.
         */
        virtual ba::awaitable<std::vector<uint8_t>> read() = 0;

        /**
         * @brief Set RF transceiver configuration option.
         *
         * @param option Option name.
         * @param value Option value as string.
         *
         * @return Awaitable with true on success, false on failure.
         */
        virtual ba::awaitable<bool> setOption(std::string option, std::string value) = 0;

        /**
         * @brief Get RF transceiver configuration option.
         *
         * @param option Option name.
         *
         * @return Awaitable with option value, or empty string on error.
         */
        virtual ba::awaitable<std::vector<uint8_t>> getOption(std::string option) = 0;

        /**
         * @brief Get all RF transceiver configuration options.
         *
         * @return Awaitable with formatted string "option:value,option:value,...".
         */
        virtual ba::awaitable<std::string> getAllOptions() = 0;

        /**
         * @brief Get required delay between write operations.
         *
         * @return Required delay in ms.
         */
        virtual std::chrono::milliseconds getRequiredWriteDelay() = 0;

        /**
         * @brief Check if RF transceiver is multichannel.
         *
         * @return true if RF transceiver supports multiple RF channels.
         */
        virtual bool isMultiChannel() = 0;

        /// String used by setOption when changing channels if transceiver supports it
        static constexpr std::string_view msCHANNEL_OPTION_STRING = "channel";

    protected:
        std::shared_ptr<su::Logger> mpLogger;
    };

}