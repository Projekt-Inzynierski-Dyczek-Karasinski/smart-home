#pragma once

// functions in PowerManager are exclusively for ESP32
#ifndef ESP32_BOARD
#error "Power management not implemented"
#endif

#include <Arduino.h>
#include <memory>

#include "utils/logger.h"

namespace ul = Utils::Logging;

namespace UniversalModuleSystem {

    /**
     * @brief Class responsible for managing power-related operations, including sleep and battery monitoring, on ESP32 boards.
     *
     * @warning Works only with ESP32.
     */
    class PowerManager {
    public:
        /**
         * @brief Gets the singleton instance of PowerManager.
         *
         * @param logger Shared pointer to the logger instance.
         * @return Reference to the PowerManager instance.
         */
        static PowerManager& getInstance(const std::shared_ptr<ul::Logger> &logger);

        // Delete copy constructor and assignment operator
        PowerManager(const PowerManager&) = delete;
        PowerManager& operator = (const PowerManager&) = delete;


        /**
         * @brief Puts the ESP32 into sleep mode for a specified duration.
         *
         * @param milliSeconds Duration to sleep in milliseconds.
         * @param enableWakeUpWithRfModule Enables wake-up through RF module, default: false.
         * @note Always enables waking up module by pressing PairingButton.
         */
        void enterSleep(uint32_t milliSeconds, bool enableWakeUpWithRfModule = false);

        /**
         * @brief Restarts the ESP32.
         * @details Calls <code>waitAndDisableCriticalFeatures()</code> before restarting ESP32
         *          to ensure nothing critical is interrupted.
         *
         * @param source String describing the source of the restart (for log).
         */
        void safeRestart(const char *source) const;

        /**
         * @brief Gets the value of the battery voltage output, read on startup.
         *
         * @return Battery voltage output (0 - 4095 -> 0V - 3.3V).
         * @warning Waits (and blocks) until the measurement ends.
         */
        uint16_t getBatteryRead() const;

        /**
         * @brief Disables the automatic sleep (if enabled) after waking up ESP by rf module.
         */
        void disableAutoSleep();

    private:
        /**
         * @brief Private constructor for singleton pattern.
         * @details Logs wake up reason, starts auto sleep logic (if enabled),
         *          initializes FreeRTOS semaphore and task for reading battery.
         *
         * @param logger Shared pointer to the logger instance.
         */
        explicit PowerManager(const std::shared_ptr<ul::Logger> &logger);

        /**
         * @brief Deletes FreeRTOS resources.
         * @warning Destructor of this class exists only for programming principles. This class should never be deleted.
         */
        ~PowerManager();

        /**
         * @brief Waits for completion and ensures that features do <b>not</b> enter in critical state.
         * @details Waits for completion and disables (in that order):
         * - RF transmission
         * - Editing data in SPIFFS
         * - Serial printing
         *
         * @warning Waiting for end of printing Serial data is necessary for putting ESP in deep sleep.
         */
        void waitAndDisableCriticalFeatures() const;

        /**
         * @brief Logs wake-up reason and, if caused by RF module, starts auto-sleep feature (if enabled).
         */
        void handleWakeUpReason();

        /**
         * @brief Initiates a FreeRTOS task for battery reading.
         */
        void readBattery();

        /**
         * @brief FreeRTOS task for asynchronous battery reading.
         *
         * @param parameters FreeRTOS task parameters (used for passing pointer to instance of PowerManager).
         * @note Deletes itself after readding is ended.
         */
        static void batteryReadTask(void* parameters);

        /**
         * @brief Enables the automatic sleep functionality.
         * @note Works only if <code>AUTO_SLEEP</code> is defined in the config.
         */
        void enableAutoSleep();

        /**
        * @brief FreeRTOS timer callback to automatically put ESP in sleep.
        * @param xTimer Handle to the timer triggering auto-sleep (used for passing pointer to instance of PowerManager).
        */
        static void goToAutoSleep(TimerHandle_t xTimer);

        /**
         * @brief Returns the time passed from powering on ESP.
         * @return Time in milliseconds.
         */
        uint32_t getCurrentTime() const;

        std::shared_ptr<ul::Logger> mpLogger;
        std::atomic<uint16_t> mBatteryRead{0}; // TODO add convertion from analog read to voltage

        SemaphoreHandle_t mReadCompleteSemaphore = nullptr; ///< FreeRTOS semaphore to indicate completion of battery read operation.

        TimerHandle_t mAutoSleepTimer = nullptr; ///< FreeRTOS timer handle for managing auto-sleep functionality.

        // auto sleep
        static uint32_t msSleepStart; ///< When sleep starts, variable saved in RTC memory.
        static int64_t msIntendedSleepTime; ///< How long sleep should last, variable saved in RTC memory.
    };
}
