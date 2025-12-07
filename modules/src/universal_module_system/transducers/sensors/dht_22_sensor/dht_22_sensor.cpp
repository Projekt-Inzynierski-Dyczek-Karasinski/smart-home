#include "dht_22_sensor.h"

#include <DHT.h>

namespace UniversalModuleSystem::Transducers {
    Dht22Sensor::Dht22Sensor(const std::shared_ptr<ul::Logger> &logger) : Sensor(logger) {}

    uint32_t Dht22Sensor::getReading() {
        xSemaphoreTake(mReadingCompleteSemaphore, portMAX_DELAY);
        xSemaphoreGive(mReadingCompleteSemaphore);

        // if reading was newer started
        if (mHumidity.load() == 0 && mTemperature.load() == 0) {
            startReading();
            xSemaphoreTake(mReadingCompleteSemaphore, portMAX_DELAY);
            xSemaphoreGive(mReadingCompleteSemaphore);
        }

        // TODO !pr change after adding new API format
        if (mHumidity.load() == -1) {
            mpLogger->error("Dht22Sensor reading", "Failed to read sensor.");
            return 0; // read failed
        }
        mpLogger->infov("Dht22Sensor reading", "Humidity (%): ", mHumidity.load());
        mpLogger->infov("Dht22Sensor reading", "Temperature (C): ", mTemperature.load());
        return 1; // read success
    }

    void Dht22Sensor::startReading() {
        xSemaphoreTake(mReadingCompleteSemaphore, 0); // make sure that semaphore indicates that reading is not completed
        xTaskCreate(
            dht22ReadTask,
            "Dht22Sensor Read Task",
            LIGHT_READ_TASK_SIZE,
            this,
            DHT22_READ_TASK_SIZE,
            nullptr
        );
    }

    void Dht22Sensor::dht22ReadTask(void *parameters) {
        auto& dht = *static_cast<Dht22Sensor*>(parameters);
        DHT dhtLib(dht.mCommonSensorData.readPin, DHT22);

        vTaskDelay(pdMS_TO_TICKS(1000));
        dhtLib.begin();
        vTaskDelay(pdMS_TO_TICKS(100));

        dht.mHumidity.store(dhtLib.readHumidity());
        dht.mTemperature.store(dhtLib.readTemperature());

        dht.mHumidity.store(dhtLib.readHumidity());
        dht.mTemperature.store(dhtLib.readTemperature());

        xSemaphoreGive(dht.mReadingCompleteSemaphore);

        uint32_t tmp = uxTaskGetStackHighWaterMark(nullptr);
        dht.mpLogger->warningv("Dht22Sensor TMP", "Task watermark:", tmp*4); // TODO !pr remove
        vTaskDelete(nullptr);
    }

}
