#pragma once

#include "universal_module_system/transducers/sensors/sensor.h"

namespace UniversalModuleSystem::Transducers {
    /**
     * @brief BME280 sensor class for measuring humidity, pressure and temperature.
     */
    class BME280 : public Sensor {
    public:
        /**
        * @brief Construct the BME280 object.
        * @param logger Shared pointer to logger.
        */
        explicit BME280(const std::shared_ptr<ul::Logger> &logger);

        /**
         * @brief Waits until the sensor reading completes.
         * @details It is used when only waiting for the reading to finish is needed, but not the reading result.
         * <code>getApiFormattedReading</code> automatically waits for the reading to finish before returning the result.
         */
        void waitUntilReadingEnds() override;

        /**
         * @brief Get humidity, pressure and temperature reading.
         * @return Vector with humidity, pressure and temperature reading as APIParameters.
         *
         * @note Thread-safe.
         */
        std::vector<API::APIParameterVariant> getApiFormattedReading() override;

        /**
         * @brief Begin an asynchronous measurement of the BME280 Sensor.
         * @details Creates a FreeRTOS task to perform reading values from the sensor.
         * This task deletes itself after the measurement ends.
         */
        void startReading() override;

    private:
        /**
         * @brief Structure holding specific data to BME280.
         */
        struct AdditionalData {
            /**
             * @brief Construct AdditionalData from JSON parameters.
             * @param json JSON object with sensor data.
             */
            explicit AdditionalData(const nl::json& json);
            AdditionalData() = default;

            // BME280 specific parameters
            uint8_t readPin2 = 0; // (bme pins: 1 - vcc, 2 - gnd, 3 - readPin, 4 - readPin2)
            uint8_t i2cAddress = 0;
            bool isLoaded = false;

        private:
            // JSON keys
            static constexpr char ms_DATA[] = "additional";
            static constexpr char ms_SECOND_READ_PIN[] = "readPin2";
            static constexpr char ms_I2C_ADDRESS[] = "I2CAddress";
        };

        /**
         * @brief Load additional BME280 sensor configuration from JSON.
         *
         * @param jsonData JSON object containing sensor parameters.
         * @return True if additional data loaded successfully, false otherwise.
         *
         * @warning <b>Not</b> thread-safe. Must be protected externally with <code>mSensorDataMutex</code> before calling.
         */
        bool loadAdditionalData(const nl::json &jsonData) override;

        static void bmeReadTask(void *parameters);

        std::atomic<float> mHumidity{0};
        std::atomic<float> mTemperature{0};
        std::atomic<float> mPressure{0};
        AdditionalData mAdditionalData{};
    };
}
