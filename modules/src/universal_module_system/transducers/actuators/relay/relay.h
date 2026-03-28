#pragma once

#include "universal_module_system/transducers/actuators/actuator.h"
#include <atomic>

namespace UniversalModuleSystem::Transducers {
    /// @brief Supported relay operations
    enum class relayOperations : uint8_t {
        UNDEFINED = 0,
        TURN_ON = 1,
        TURN_OFF = 2,
    };

    /**
     * @brief Relay actuator implementation.
     *
     * @details Remembers relay state and holds pin logic across deep sleep.
     *
     * @warning Due to RTC memory limitations, relay IDs assigned in the config must be in the range 0–31.
     */
    class Relay final : public Actuator {
    public:
        /**
         * @brief Construct a relay actuator.
         *
         * @param logger Shared pointer to the logger instance.
         */
        explicit Relay(const std::shared_ptr<ul::Logger> &logger);

        /**
         * @brief Get current relay state.
         *
         * @return ActuatorState or an error.
         */
        apiPv getState() override;

        /**
         * @brief Toggle relay output between ON and OFF.
         *
         * @return New actuatorState or an error.
         */
        apiPv toggle() override;

        /**
         * @brief Execute an explicit relay operation (TURN_ON / TURN_OFF).
         * @param operation Operation request.
         * @return Operation result or an error.
         */
        apiPv doOperation(apiPv operation) override;


        /**
         * @brief Boot-time handler for relay.
         * @details Ensures the relay is off after restart - restart, not wake up from deep sleep.
         * A software restart does not reset the pin logic on its own.
         *
         * @param jsonData JSON configuration used to initialize the relay.
         */
        void onBoot(const nl::json &jsonData) override;

    private:
        /**
         * @brief Read relay state from the persistent bitmask.
         *
         * @return Current actuator state.
         *
         * @throws std::invalid_argument if the relay id would exceed the bitmask capacity.
         */
        [[nodiscard]] actuatorState getStatePrivate() const;

        /**
         * @brief Drive relay GPIO output and update persistent state.
         *
         * @param state Desired output state.
         *
         * @note Thread-safe.
         */
        void changePinOutput(bool state);

        /**
         * @brief Bitmask storing relay states (bit per relay id).
         * @details Declared static due to limitations with RTC memory.
         */
        static std::atomic<uint32_t> relayStateBitmask;
    };
}