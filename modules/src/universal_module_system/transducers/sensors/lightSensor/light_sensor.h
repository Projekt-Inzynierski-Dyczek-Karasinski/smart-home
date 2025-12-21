#pragma once

#include "universal_module_system/transducers/sensors/sensor.h"

namespace UniversalModuleSystem::Transducers {
    class LightSensor final : public Sensor {
    public:
        /**
         * @brief Construct the LightSensor object.
         * @param logger Shared pointer to logger.
         */
        explicit LightSensor(const std::shared_ptr<ul::Logger> &logger);

        /**
         * @brief Get the sensor reading.
         * @return Sensor reading.
         *
         * @note Thread-safe.
         */
        std::vector<API::APIParameterVariant> getApiFormattedReading() override;

        /**
         * @brief Begin an asynchronous measurement of the sensor.
         * @details Creates a FreeRTOS task to perform multiple ADC samples and stores the calculated voltage.
         * This task deletes itself after end measurement.
         */
        void startReading() override;

    private:
        uint8_t calculateLightPercentage(uint16_t rawReading);

        /**
        * @brief FreeRTOS task function to perform photoresistor ADC readings.
        * @details Takes multiple readings, averages them, and converts raw ADC to the voltage output from photoresistor.
        *
        * @param parameters FreeRTOS task parameters.
        *
        * @note Deletes itself after end of reading.
        */
        static void lightReadTask(void *parameters);

        std::atomic<uint8_t> mLightPercentage{0};
    };
}
