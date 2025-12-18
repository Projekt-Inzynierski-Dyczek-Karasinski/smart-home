#pragma once

#ifndef ESP32_BOARD
#error "BatterySensorESP32 class is exclusively for ESP32"
#endif

#include <atomic>

#include "universal_module_system/transducers/sensors/sensor.h"


namespace UniversalModuleSystem::Transducers {
    /**
     * @brief Battery sensor class for measuring battery voltage on ESP32.
     */
    class BatterySensorESP32 final : public Sensor {
    public:
        /**
         * @brief Construct the BatterySensorESP32 object.
         * @param logger Shared pointer to logger.
         */
        explicit BatterySensorESP32(const std::shared_ptr<ul::Logger> &logger);

        /**
         * @brief Get battery percentage reading.
         * @return Battery voltage.
         *
         * @note Thread-safe.
         */
        API::APIParameterVariant getApiFormattedReading() override;

        /**
         * @brief Begin an asynchronous measurement of the battery voltage.
         * @details Creates a FreeRTOS task to perform multiple ADC samples and stores the calculated voltage.
         * This task deletes itself after end measurement.
         */
        void startReading() override;

    private:
        /**
         * @brief Load additional battery sensor configuration from JSON.
         *
         * @param jsonData JSON object containing sensor parameters.
         * @return True if additional data loaded successfully, false otherwise.
         *
         * @warning <b>Not</b> thread-safe. Must be protected externally with <code>mSensorDataMutex</code> before calling.
         */
        bool loadAdditionalData(const nl::json &jsonData) override;

        /**
         * @brief FreeRTOS task function to perform battery ADC readings.
         * @details Takes multiple readings, averages them, and converts raw ADC to the voltage output from battery.
         *
         * @param parameters FreeRTOS task parameters.
         *
         * @note Deletes itself after end of reading.
         */
        static void batteryReadTask(void *parameters);

        /**
         * @brief Calculates voltage in millivolts (mV) from raw ADC value.
         * @details Accounts for voltage divider and returns real battery voltage output.
         *
         * @param rawAnalogRead Raw ADC reading.
         * @return Calculated battery voltage in mV.
         */
        uint16_t calculateVoltage(uint16_t rawAnalogRead) const;

        /**
         * @brief Calculates the battery charge percentage, based on output voltage.
         * @param outputVoltage Battery output voltage in mV.
         * @return The battery charge percentage.
         */
        uint8_t calculateBatteryChargePercentage(uint16_t outputVoltage) const;

        /**
         * @brief Structure holding specific data to BatterySensorESP32.
         */
        struct AdditionalData {
            /**
             * @brief Construct AdditionalData from JSON parameters.
             * @param json JSON object with sensor data.
             */
            explicit AdditionalData(const nl::json& json);
            AdditionalData() = default;

            // BatterySensorESP32 specific parameters
            uint32_t vccResistor = 0;
            uint32_t gndResistor = 0;
            uint32_t minVoltage = 0; ///< in mV
            uint32_t maxVoltage = 0; ///< in mV
            bool isLoaded = false;

        private:
            // JSON keys
            static constexpr char ms_DATA[] = "additional";
            static constexpr char ms_VCC_RESISTOR_DATA[] = "rVCC";
            static constexpr char ms_GND_RESISTOR_DATA[] = "rGND";
            static constexpr char ms_V_MIN_DATA[] = "Vmin";
            static constexpr char ms_V_MAX_DATA[] = "Vmax";
        };

        AdditionalData mAdditionalData{};
        std::atomic<uint8_t> mBatteryReadPercentage{0};
    };
}
