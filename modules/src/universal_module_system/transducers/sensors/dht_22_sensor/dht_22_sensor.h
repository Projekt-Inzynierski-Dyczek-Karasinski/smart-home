#pragma once

#include "universal_module_system/transducers/sensors/sensor.h"

namespace UniversalModuleSystem::Transducers {
    /**
     * @brief DHT22 sensor class for measuring humidity and temperature.
     */
    class Dht22Sensor final : public Sensor {
    public:
        /**
         * @brief Construct the DHT22Sensor object.
         * @param logger Shared pointer to logger.
         */
        explicit Dht22Sensor(const std::shared_ptr<ul::Logger> &logger);

        /**
         * @brief Waits until the sensor reading completes.
         * @details It is used when only waiting for the reading to finish is needed, but not the reading result.
         * <code>getApiFormattedReading</code> automatically waits for the reading to finish before returning the result.
         */
        void waitUntilReadingEnds() override;

        /**
         * @brief Get humidity and temperature reading.
         * @return Vector with humidity and temperature reading as APIParameters.
         *
         * @note Thread-safe.
         */
        std::vector<API::APIParameterVariant> getApiFormattedReading() override;

        /**
         * @brief Begin an asynchronous measurement of the DHT22 Sensor.
         * @details Creates a FreeRTOS task to perform reading values from the sensor.
         * This task deletes itself after the measurement ends.
         */
        void startReading() override;

    private:
        static void dht22ReadTask(void *parameters);

        std::atomic<float> mHumidity{0};
        std::atomic<float> mTemperature{0};
    };
}
