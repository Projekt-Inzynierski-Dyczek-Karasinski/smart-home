#pragma once

#include <Arduino.h>

namespace UniversalModuleSystem {
// TODO !pr update comments

    /**
     * @brief Class responsible for managing power-related operations, including sleep and battery monitoring, on ESP32 boards.
     *
     * @warning Works only with ESP32.
     */
    class IPowerManager {
    public:
        // Delete copy constructor and assignment operator
        IPowerManager(const IPowerManager&) = delete;
        IPowerManager& operator = (const IPowerManager&) = delete;

        /**
         * @brief Puts the ESP32 into sleep mode for a specified duration.
         *
         * @param milliSeconds Duration to sleep in milliseconds.
         * @param enableWakeUpWithRfModule Enables wake-up through RF module, default: false.
         * @note Always enables waking up module by pressing PairingButton.
         */
        virtual void enterSleep(uint32_t milliSeconds, bool enableWakeUpWithRfModule) = 0;

        /**
         * @brief Restarts the ESP32.
         * @details Calls <code>waitAndDisableCriticalFeatures()</code> before restarting ESP32
         *          to ensure nothing critical is interrupted.
         *
         * @param source String describing the source of the restart (for log).
         */
        virtual void safeRestart(const char *source) const = 0;

        /**
         * @brief Gets the value of the battery voltage output, read on startup.
         *
         * @return Battery voltage output (0 - 4095 -> 0V - 3.3V).
         * @warning Waits (and blocks) until the measurement ends.
         */
        virtual uint16_t getBatteryRead() const = 0;

        /**
         * @brief Disables the automatic sleep (if enabled) after waking up ESP by rf module.
         */
        virtual void disableAutoSleep() = 0;

    protected:
        /**
         * @brief Private constructor for singleton pattern.
         * @details Logs wake up reason, starts auto sleep logic (if enabled),
         *          initializes FreeRTOS semaphore and task for reading battery.
         *
         * @param logger Shared pointer to the logger instance.
         */
        IPowerManager() = default;

        /**
         * @brief Deletes FreeRTOS resources.
         * @warning Destructor of this class exists only for programming principles. This class should never be deleted.
         */
        virtual ~IPowerManager() = default;
    };
}
