#pragma once

// functions in PowerManagerESP32 are exclusively for ESP32
#ifndef ESP32_BOARD
#error "Power management not implemented"
#endif

#include "i_power_manager.h"

#include <memory>

#include "../../utils/logger.h"

namespace ul = Utils::Logging;

namespace UniversalModuleSystem {

    /**
     * @brief Class responsible for managing power-related operations, including sleep and battery monitoring, on ESP32 boards.
     *
     * @warning Works only with ESP32.
     */
    class PowerManagerESP32 final : public IPowerManager {
    public:
        /**
         * @brief Gets the singleton instance of PowerManagerESP32.
         *
         * @param logger Shared pointer to the logger instance.
         * @return Reference to the PowerManagerESP32 instance.
         */
        static PowerManagerESP32& getInstance(const std::shared_ptr<ul::Logger> &logger);

        // Delete copy constructor and assignment operator
        PowerManagerESP32(const PowerManagerESP32&) = delete;
        PowerManagerESP32& operator = (const PowerManagerESP32&) = delete;


        /**
         * @brief Puts the ESP32 into sleep mode for a specified duration.
         *
         * @param milliSeconds Duration to sleep in milliseconds.
         * @param enableWakeUpWithRfModule Enables wake-up through RF module.
         * @note Always enables waking up module by pressing PairingButton.
         */
        void enterSleep(uint32_t milliSeconds, bool enableWakeUpWithRfModule) override;

        /**
         * @brief Restarts the ESP32.
         * @details Calls <code>waitAndDisableCriticalFeatures()</code> before restarting ESP32
         *          to ensure nothing critical is interrupted.
         *
         * @param source String describing the source of the restart (for log).
         */
        void safeRestart(const char *source) const override;

        /**
         * @brief Gets the value of the battery voltage output, read on startup.
         *
         * @return Battery voltage output (0 - 4095 -> 0V - 3.3V).
         * @warning Waits (and blocks) until the measurement ends.
         */
        uint16_t getBatteryRead() const override;

        /**
         * @brief Disables the automatic sleep (if enabled) after waking up ESP by rf module.
         */
        void disableAutoSleep() override;

    private:
        /**
         * @brief Private constructor for singleton pattern.
         * @details Logs wake up reason, starts auto sleep logic (if enabled),
         *          initializes FreeRTOS semaphore and task for reading battery.
         *
         * @param logger Shared pointer to the logger instance.
         */
        explicit PowerManagerESP32(const std::shared_ptr<ul::Logger> &logger);

        /**
         * @brief Deletes FreeRTOS resources.
         * @warning Destructor of this class exists only for programming principles. This class should never be deleted.
         */
        ~PowerManagerESP32() override;

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
         * @param parameters FreeRTOS task parameters (used for passing pointer to instance of PowerManagerESP32).
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
        * @param xTimer Handle to the timer triggering auto-sleep (used for passing pointer to instance of PowerManagerESP32).
        */
        static void goToAutoSleep(TimerHandle_t xTimer);

        /**
         * @brief Returns the time passed from powering on ESP.
         * @return Time in milliseconds.
         */
        uint32_t getCurrentTime() const;

        std::atomic<uint16_t> mBatteryRead{0}; // TODO add convertion from analog read to voltage
        std::shared_ptr<ul::Logger> mpLogger;

        SemaphoreHandle_t mReadCompleteSemaphore = nullptr; ///< FreeRTOS semaphore to indicate completion of battery read operation.

        TimerHandle_t mAutoSleepTimer = nullptr; ///< FreeRTOS timer handle for managing auto-sleep functionality.

        // auto sleep
        static uint32_t msSleepStart; ///< When sleep starts, variable saved in RTC memory.
        static int64_t msIntendedSleepTime; ///< How long sleep should last, variable saved in RTC memory.
    };
}