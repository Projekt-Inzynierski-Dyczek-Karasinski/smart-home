#pragma once

#include "universal_module_system/transducers/sensors/sensor.h"

namespace UniversalModuleSystem::Transducers {
    /**
     * @brief Window sensor state.
     */
    enum class windowSensorStatus: uint8_t {
        CLOSED = 0,
        OPEN = 1,
    };

    // FIXME WindowSensor sometimes works only after first deep sleep.

    /**
     * @brief Sensor that reports whether a window is open or closed.
     * @details If "canAwake" is set to true in base_config.json, WindowSensor will wake up
     * the ESP32 when the window state changes.
     *
     * @warning Due to limitations of the ESP32's EXT0 and EXT1 wake-up sources,
     * only one sensor is allowed to wake up the ESP32.
     */
    class WindowSensor final : public Sensor {
    public:
        /**
         * @brief Construct the WindowSensor object.
         * @details Configures the read pin as INPUT_PULLUP.
         *
         * @param logger Shared pointer to the logger instance.
         */
        explicit WindowSensor(const std::shared_ptr<ul::Logger> &logger);

        /**
         * @brief Get formatted reading (APIParameterVariant).
         * @details Reads the configured pin and returns CLOSED/OPEN status.
         *
         * @return Vector of readings in API format.
         */
        std::vector<API::APIParameterVariant> getApiFormattedReading() override;

        /**
         * @brief Called before the device goes to sleep, tries to add wake-up source on EXT0.
         *
         * @note Thread-safe.
         */
        void onSleep() override;

        /**
         * @brief Start acquiring data from the sensor.
         * @details No-op for this sensor.
         */
        void startReading() override;

        /**
         * @brief Wait until the sensor reading completes.
         * @details No-op for this sensor.
         */
        void waitUntilReadEnds() override;
    };
}
