#include "light_sensor.h"

namespace UniversalModuleSystem::Transducers {
    LightSensor::LightSensor(const std::shared_ptr<ul::Logger> &logger) : Sensor(logger) {}

    uint32_t LightSensor::getReading() {
        xSemaphoreTake(mReadingCompleteSemaphore, portMAX_DELAY);
        xSemaphoreGive(mReadingCompleteSemaphore);
        // if reading was newer started
        if (mLightPercentage.load() == 0) {
            startReading();
            xSemaphoreTake(mReadingCompleteSemaphore, portMAX_DELAY);
            xSemaphoreGive(mReadingCompleteSemaphore);
        }
        return mLightPercentage.load();
    }

    void LightSensor::startReading() {
        xSemaphoreTake(mReadingCompleteSemaphore, 0); // make sure that semaphore indicates that reading is not completed
        xTaskCreate(
            lightReadTask,
            "Light Read Task",
            LIGHT_READ_TASK_SIZE,
            this,
            LOW_TASK_PRIORITY,
            nullptr
        );
    }

    uint8_t LightSensor::calculateLightPercentage(const uint16_t rawReading) {
        constexpr uint16_t MAX_ANALOG_READ = 4096;

        // uint32_t to not exceed overflow in calculations
        uint32_t result = rawReading;
        result *= 100;
        result /= MAX_ANALOG_READ;

        return (uint8_t)result;
    }


    void LightSensor::lightReadTask(void *parameters) {
        auto& bs = *static_cast<LightSensor*>(parameters);
        constexpr uint16_t TIME_BETWEEN_READS = 50; // ms
        constexpr uint8_t NUMBER_OF_READS = 5;
        uint16_t lightReadSum = 0;

        xSemaphoreTake(bs.mSensorDataMutex, portMAX_DELAY);

        if (bs.mCommonSensorData.powerPin != 0) {
            pinMode(bs.mCommonSensorData.powerPin, OUTPUT);
            digitalWrite(bs.mCommonSensorData.powerPin, HIGH);
        }

        for (uint8_t i = 0; i < NUMBER_OF_READS; i++) {
            vTaskDelay(pdMS_TO_TICKS(TIME_BETWEEN_READS));
            lightReadSum += analogRead(bs.mCommonSensorData.readPin);
        }
        if (bs.mCommonSensorData.powerPin != 0) {
            digitalWrite(bs.mCommonSensorData.powerPin, LOW);
        }

        bs.mLightPercentage.store(bs.calculateLightPercentage(lightReadSum / NUMBER_OF_READS));
        xSemaphoreGive(bs.mSensorDataMutex);

        bs.mpLogger->verbosev("LightSensor Task", "Light (%): ", bs.mLightPercentage.load());
        xSemaphoreGive(bs.mReadingCompleteSemaphore);

        vTaskDelete(nullptr);
    }

    bool LightSensor::loadAdditionalData(const nl::json &jsonData) {
        // bool isLoadedSuccessfully = true;
        // try {
        //     mAdditionalData = AdditionalData(jsonData);
        // } catch (...) {
        //     mpLogger->error("BatterySensorESP32 class", "Failed to load additional sensor data.");
        //     isLoadedSuccessfully = false;
        // }
        return true;
    }

    // LightSensor::AdditionalData::AdditionalData(const nl::json &json) :
    //     vccResistor(json[ms_DATA][ms_VCC_RESISTOR_DATA]),
    //     isLoaded(true) {}
}
