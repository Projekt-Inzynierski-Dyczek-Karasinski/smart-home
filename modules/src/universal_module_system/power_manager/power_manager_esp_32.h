#pragma once

#ifndef ESP32_BOARD
#error "PowerManagerESP32 class is exclusively for ESP32"
#endif

#include "i_power_manager.h"

#include <atomic>
#include <memory>
#include <nlohmann/json.hpp>

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
        void enterSleep(uint32_t milliSeconds, bool enableWakeUpWithRfModule) override __attribute__((noreturn));

        /**
         * @brief Restarts the ESP32.
         * @details Calls <code>waitAndDisableCriticalFeatures()</code> before restarting ESP32
         *          to ensure nothing critical is interrupted.
         *
         * @param source String describing the source of the restart (for log).
         */
        void safeRestart(const char *source) const override __attribute__((noreturn));

        /**
         * @brief Disables the automatic sleep (if enabled) after waking up ESP by rf module.
         */
        void disableAutoSleep() override;

        /**
         * @brief Restarts idle timer, if the idle timer is enabled.
         */
        void restartIdleTimer() override;

        /**
         * @brief Checks whether the module booted after a reset or after waking from deep sleep.
         * @return True if the module booted after a reset, false if it booted after waking from deep sleep.
         */
        [[nodiscard]] bool wasModuleRestarted() const override;

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

        /**
         * @brief Creates idle timer, that automatically put ESP32 to sleep, if idle autosleep is enabled.
         */
        void createIdleTimer();

        /**
         * @brief Idle timer callback.
         * @param xTimer Handle to the timer triggering auto-sleep (used for passing pointer to instance of PowerManagerESP32).
         */
        static void idleAutosleep(TimerHandle_t xTimer);

        std::shared_ptr<ul::Logger> mpLogger;

        // TODO remove?
        TimerHandle_t mAutoSleepTimer = nullptr; ///< FreeRTOS timer handle for managing auto-sleep functionality.

        TimerHandle_t mIdleTimer = nullptr; ///< FreeRTOS timer handle for managing idle auto-sleep functionality.

        std::atomic<uint32_t> mIdleSleepTime{0};

        // auto sleep
        static uint32_t msSleepStart; ///< When sleep starts, variable saved in RTC memory.
        static int64_t msIntendedSleepTime; ///< How long sleep should last, variable saved in RTC memory.


        // JSON keys
        static constexpr char ms_IDLE_TIMER_DATA[] = "idleAutoSleep";
        static constexpr char ms_IDLE_TIMER_TIMEOUT[] = "idleTimeout";
        static constexpr char ms_IDLE_TIMER_SLEEP_TIME[] = "sleepTime";
    };
}