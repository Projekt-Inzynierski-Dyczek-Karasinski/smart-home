#include "battery_sensor_esp_32.h"

namespace UniversalModuleSystem::Transducers {
    BatterySensorESP32::BatterySensorESP32(const std::shared_ptr<ul::Logger> &logger) : Sensor(logger) {}

    API::APIParameterVariant BatterySensorESP32::getApiFormattedReading() {
        xSemaphoreTake(mReadingCompleteSemaphore, portMAX_DELAY);
        xSemaphoreGive(mReadingCompleteSemaphore);
        // if reading was newer started
        if (mBatteryReadPercentage.load() == 0) {
            startReading();
            xSemaphoreTake(mReadingCompleteSemaphore, portMAX_DELAY);
            xSemaphoreGive(mReadingCompleteSemaphore);
        }
        return API::APIParameter(mBatteryReadPercentage.load());
    }

    uint32_t BatterySensorESP32::getReadingOLD() {
        xSemaphoreTake(mReadingCompleteSemaphore, portMAX_DELAY);
        xSemaphoreGive(mReadingCompleteSemaphore);
        // if reading was newer started
        if (mBatteryReadPercentage.load() == 0) {
            startReading();
            xSemaphoreTake(mReadingCompleteSemaphore, portMAX_DELAY);
            xSemaphoreGive(mReadingCompleteSemaphore);
        }
        return mBatteryReadPercentage.load();
    }

    void BatterySensorESP32::startReading() {
        xSemaphoreTake(mReadingCompleteSemaphore, 0); // make sure that semaphore indicates that reading is not completed
        xTaskCreate(
            batteryReadTask,
            "Battery Read Task",
            BATTERY_READ_TASK_SIZE,
            this,
            LOW_TASK_PRIORITY,
            nullptr
        );
    }

    uint16_t BatterySensorESP32::calculateVoltage(const uint16_t rawAnalogRead) const {
        constexpr uint16_t MAX_ANALOG_READ = 4095;
        constexpr uint16_t MAX_ANALOG_READ_MILLIVOLTAGE = 3300;

        // uint64_t to not exceed overflow in calculations
        uint64_t result = rawAnalogRead;
        result *= MAX_ANALOG_READ_MILLIVOLTAGE;
        result *= (mAdditionalData.vccResistor + mAdditionalData.gndResistor);
        result /= mAdditionalData.gndResistor;
        result /= MAX_ANALOG_READ;

        return (uint16_t)result;
    }

    uint8_t BatterySensorESP32::calculateBatteryChargePercentage(const uint16_t outputVoltage) const {
        if (outputVoltage <= mAdditionalData.minVoltage) return 0;
        if (outputVoltage >= mAdditionalData.maxVoltage) return 100;

        // uint32_t to not exceed overflow in calculations
        uint32_t result = outputVoltage - mAdditionalData.minVoltage;
        result *= 100;
        result /= (mAdditionalData.maxVoltage - mAdditionalData.minVoltage);

        return (uint8_t)result;
    }


    void BatterySensorESP32::batteryReadTask(void *parameters) {
        auto& bs = *static_cast<BatterySensorESP32*>(parameters);
        constexpr uint16_t TIME_BETWEEN_READS = 50; // ms
        constexpr uint8_t NUMBER_OF_READS = 5;
        uint16_t batteryReadSum = 0;

        xSemaphoreTake(bs.mSensorDataMutex, portMAX_DELAY);

        for (uint8_t i = 0; i < NUMBER_OF_READS; i++) {
            vTaskDelay(pdMS_TO_TICKS(TIME_BETWEEN_READS));
            batteryReadSum += analogRead(bs.mCommonSensorData.readPin);
        }

        const uint32_t batteryVoltage = bs.calculateVoltage(batteryReadSum / NUMBER_OF_READS);
        bs.mBatteryReadPercentage.store(bs.calculateBatteryChargePercentage(batteryVoltage));
        xSemaphoreGive(bs.mSensorDataMutex);

        bs.mpLogger->verbosev("BatterySensorESP32 Task", "Battery charge level is (mV): ", batteryVoltage);
        bs.mpLogger->verbosev("BatterySensorESP32 Task", "Battery charge level is (%): ", bs.mBatteryReadPercentage.load());
        xSemaphoreGive(bs.mReadingCompleteSemaphore);

        vTaskDelete(nullptr);
    }

    bool BatterySensorESP32::loadAdditionalData(const nl::json &jsonData) {
        bool isLoadedSuccessfully = true;
        try {
            mAdditionalData = AdditionalData(jsonData);
        } catch (...) {
            mpLogger->error("BatterySensorESP32 class", "Failed to load additional sensor data.");
            isLoadedSuccessfully = false;
        }
        return isLoadedSuccessfully;
    }

    BatterySensorESP32::AdditionalData::AdditionalData(const nl::json &json) :
        vccResistor(json[ms_DATA][ms_VCC_RESISTOR_DATA]),
        gndResistor(json[ms_DATA][ms_GND_RESISTOR_DATA]),
        minVoltage(json[ms_DATA][ms_V_MIN_DATA]),
        maxVoltage(json[ms_DATA][ms_V_MAX_DATA]),
        isLoaded(true) {}
}
