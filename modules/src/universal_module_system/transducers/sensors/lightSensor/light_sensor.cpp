#include "light_sensor.h"

namespace UniversalModuleSystem::Transducers {
    LightSensor::LightSensor(const std::shared_ptr<ul::Logger> &logger) : Sensor(logger) {
    }

    std::vector<API::APIParameterVariant> LightSensor::getApiFormattedReading() {
        xSemaphoreTake(mReadingCompleteSemaphore, portMAX_DELAY);
        xSemaphoreGive(mReadingCompleteSemaphore);
        // if reading was newer started
        if (mLightPercentage.load() == 0) {
            startReading();
            xSemaphoreTake(mReadingCompleteSemaphore, portMAX_DELAY);
            xSemaphoreGive(mReadingCompleteSemaphore);
        }

        return std::vector<API::APIParameterVariant> {API::APIParameter(mLightPercentage.load())};
    }

    void LightSensor::startReading() {
        xSemaphoreTake(mReadingCompleteSemaphore, 0);
        // make sure that semaphore indicates that reading is not completed
        xTaskCreate(
            lightReadTask,
            "Light Read Task",
            DHT22_READ_TASK_SIZE,
            this,
            HIGH_TASK_PRIORITY,
            nullptr
        );
    }

    uint8_t LightSensor::calculateLightPercentage(const uint16_t rawReading) {
        constexpr uint16_t MAX_ANALOG_READ = 4096;

        // uint32_t to not exceed overflow in calculations
        uint32_t result = rawReading;
        result *= 100;
        result /= MAX_ANALOG_READ;

        return (uint8_t) result;
    }


    void LightSensor::lightReadTask(void *parameters) {
        auto &ls = *static_cast<LightSensor *>(parameters);
        constexpr uint16_t TIME_BETWEEN_READS = 50; // ms
        constexpr uint8_t NUMBER_OF_READS = 5;
        uint16_t lightReadSum = 0;

        xSemaphoreTake(ls.mSensorDataMutex, portMAX_DELAY);

        ls.mpLogger->errorv("LightSensor test", "read: ", ls.mCommonSensorData.readPin);

        for (uint8_t i = 0; i < NUMBER_OF_READS; i++) {
            vTaskDelay(pdMS_TO_TICKS(TIME_BETWEEN_READS));
            lightReadSum += analogRead(ls.mCommonSensorData.readPin);
        }

        ls.mLightPercentage.store(ls.calculateLightPercentage(lightReadSum / NUMBER_OF_READS));
        xSemaphoreGive(ls.mSensorDataMutex);

        ls.mpLogger->verbosev("LightSensor Task", "Light (%): ", ls.mLightPercentage.load());
        xSemaphoreGive(ls.mReadingCompleteSemaphore);

        vTaskDelete(nullptr);
    }
}
