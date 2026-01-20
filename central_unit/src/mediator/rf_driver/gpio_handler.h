#pragma once

#include <gpiod.hpp>
#include <string>

namespace SmartHomeMediator {
    /**
     * @brief RAII wrapper for GPIO pin using libgpiod.
     */
    class GpioHandler {
    public:
        /**
         * @brief Create GPIO handler and claim pin.
         *
         * @param pin GPIO pin number (BCM numbering, 17 for GPIO17).
         * @param setInitialStateHigh Initial pin value.
         * @param setPinToOutput Pin mode.
         * @param chipName GPIO chip name.
         */
        GpioHandler(int pin, bool setInitialStateHigh, bool setPinToOutput, const std::string &chipName);

        ~GpioHandler() = default;

        // Prevent copying
        GpioHandler(const GpioHandler &) = delete;

        GpioHandler &operator=(const GpioHandler &) = delete;

        /**
         * @brief Set pin HIGH.
         *
         * @throws std::runtime_error on failure.
         */
        void setHigh() const;

        /**
         * @brief Set pin LOW.
         *
         * @throws std::runtime_error on failure.
         */
        void setLow() const;

        /**
         * @brief Read current pin value.
         *
         * @return 1 if HIGH, 0 if LOW
         *
         * @throws std::runtime_error on failure.
         */
        int read() const;

    private:
        gpiod::chip mChip;
        gpiod::line mLine;
        int mPin;
    };
}
