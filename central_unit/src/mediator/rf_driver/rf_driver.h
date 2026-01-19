#pragma once

#include "async_logger.h"

#include <string>
#include <string_view>
#include <map>
#include <vector>
#include <memory>

#include <boost/asio/awaitable.hpp>

namespace su = SmartHome::Utils;
namespace ba = boost::asio;

namespace SmartHomeMediator {
    /**
     * @brief Generic RF driver interface with awaitable operations.
     *
     * @details Provides abstract interface for RF modules with coroutine-based
     *          async operations. Implementations handle specific hardware (HC-12, etc).
     */
    class RfDriver {
    public:
        virtual ~RfDriver() = default;

        /**
         * @brief Write data to RF module in transparent mode.
         *
         * @param data Binary data to transmit.
         * @return Awaitable that completes when write finishes.
         * @throws boost::system::system_error on communication error.
         */
        virtual ba::awaitable<void> write(std::vector<uint8_t> data) = 0;

        /**
         * @brief Read data from RF module in transparent mode.
         *
         * @return Awaitable with received data string.
         * @throws boost::system::system_error on communication error.
         */
        virtual ba::awaitable<std::vector<uint8_t>> read() = 0;

        /**
         * @brief Set RF module configuration option.
         *
         * @param option Option name.
         * @param value Option value as string.
         * @return Awaitable with true on success, false on failure.
         */
        virtual ba::awaitable<bool> setOption(std::string option, std::string value) = 0;

        /**
         * @brief Get RF module configuration option.
         *
         * @param option Option name.
         * @return Awaitable with option value, or empty string on error.
         */
        virtual ba::awaitable<std::vector<uint8_t>> getOption(std::string option) = 0;

        /**
         * @brief Get all RF module configuration options.
         *
         * @return Awaitable with formatted string "option:value,option:value,...".
         */
        virtual ba::awaitable<std::string> getAllOptions() = 0;

        /**
         * @brief Get required delay between write operations.
         *
         * @return required delay in ms.
         */
        virtual std::chrono::milliseconds getRequiredWriteDelay() = 0;

        virtual bool isMultiChannel() = 0;

        static constexpr std::string_view msCHANNEL_OPTION_STRING = "channel";

    protected:
        std::shared_ptr<su::Logger> mpLogger;
    };

}