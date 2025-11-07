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

        uint32_t getReading() override;

        void startReading() override;

    private:
        bool loadAdditionalData(const nl::json &jsonData) override;

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

        /**
         * @brief Structure holding specific data to BatterySensorESP32.
         */
        // struct AdditionalData {
        //     /**
        //      * @brief Construct AdditionalData from JSON parameters.
        //      * @param json JSON object with sensor data.
        //      */
        //     explicit AdditionalData(const nl::json& json);
        //     AdditionalData() = default;
        //
        //     // BatterySensorESP32 specific parameters
        //     uint32_t vccResistor = 0;
        //     bool isLoaded = false;
        //
        // private:
        //     // JSON keys
        //     static constexpr char ms_DATA[] = "additional";
        //     static constexpr char ms_VCC_RESISTOR_DATA[] = "rVCC";
        // };

        // AdditionalData mAdditionalData{};
        std::atomic<uint8_t> mLightPercentage{0};
    };
}
