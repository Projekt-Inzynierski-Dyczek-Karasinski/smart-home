#pragma once

#include <Arduino.h>

namespace UniversalModuleSystem {

    /**
     * @brief Interface class responsible for managing power-related operations.
     * @note After adding new derived class add alias for it in power_manager.h file.
     */
    class IPowerManager {
    public:
        // Delete copy constructor and assignment operator
        IPowerManager(const IPowerManager&) = delete;
        IPowerManager& operator = (const IPowerManager&) = delete;

        /**
         * @brief Puts the module into sleep mode for a specified duration.
         *
         * @param milliSeconds Duration to sleep in milliseconds.
         * @param enableWakeUpWithRfModule Enables wake-up through RF module.
         * @note Always enables waking up the module by pressing PairingButton.
         */
        virtual void enterSleep(uint32_t milliSeconds, bool enableWakeUpWithRfModule) = 0;

        /**
         * @brief Ensures that features do <b>not</b> enter a critical state and restarts the module.
         *
         * @param source String describing the source of the restart (for log).
         *
         * @note This function does not return.
         */
        virtual void safeRestart(const char *source) const  __attribute__((noreturn)) = 0;

        /**
         * @brief Restarts idle timer, if the idle timer is enabled.
         */
        virtual void restartIdleTimer() = 0;

        /**
         * @brief Checks whether the module booted after a reset or after waking from deep sleep.
         * @return True if the module booted after a reset, false if it booted after waking from deep sleep.
         */
        [[nodiscard]] virtual bool wasModuleRestarted() const = 0;

    protected:
        /**
         * @brief Private constructor for singleton pattern.
         */
        IPowerManager() = default;

        /**
         * @brief Private destructor for singleton pattern.
         */
        virtual ~IPowerManager() = default;
    };
}
