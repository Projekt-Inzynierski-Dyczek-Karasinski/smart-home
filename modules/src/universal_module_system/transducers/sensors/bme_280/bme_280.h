#pragma once

#include "universal_module_system/transducers/sensors/sensor.h"

// TODO !pr add comments

namespace UniversalModuleSystem::Transducers {
    class BME280 : public Sensor {
    public:
        explicit BME280(const std::shared_ptr<ul::Logger> &logger);

        std::vector<API::APIParameterVariant> getApiFormattedReading() override;

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

        bool loadAdditionalData(const nl::json &jsonData) override;

        static void bmeReadTask(void *parameters);

        std::atomic<float> mHumidity{0};
        std::atomic<float> mTemperature{0};
        std::atomic<float> mPressure{0};
        AdditionalData mAdditionalData{};
    };
}
