#include "dht_22_sensor.h"

#include <DHT.h>

namespace UniversalModuleSystem::Transducers {
    Dht22Sensor::Dht22Sensor(const std::shared_ptr<ul::Logger> &logger) : Sensor(logger) {}

    std::vector<API::APIParameterVariant> Dht22Sensor::getApiFormattedReading() {
        xSemaphoreTake(mReadingCompleteSemaphore, portMAX_DELAY);
        xSemaphoreGive(mReadingCompleteSemaphore);

        // if reading was newer started
        if (mHumidity.load() == 0 && mTemperature.load() == 0) {
            startReading();
            xSemaphoreTake(mReadingCompleteSemaphore, portMAX_DELAY);
            xSemaphoreGive(mReadingCompleteSemaphore);
        }

        mpLogger->verbosev("Dht22Sensor reading", "Humidity (%): ", mHumidity.load());
        mpLogger->verbosev("Dht22Sensor reading", "Temperature (C): ", mTemperature.load());

        return std::vector<API::APIParameterVariant> {
            API::APIParameter(mHumidity.load()),
            API::APIParameter(mTemperature.load())
        };
    }

    void Dht22Sensor::startReading() {
        xSemaphoreTake(mReadingCompleteSemaphore, 0); // make sure that semaphore indicates that reading is not completed
        xTaskCreate(
            dht22ReadTask,
            "Dht22Sensor Read Task",
            DHT22_READ_TASK_SIZE,
            this,
            HIGH_TASK_PRIORITY,
            nullptr
        );
    }

    void Dht22Sensor::dht22ReadTask(void *parameters) {
        auto& dht = *static_cast<Dht22Sensor*>(parameters);

        xSemaphoreTake(dht.mSensorDataMutex, portMAX_DELAY);
        DHT dhtLib(dht.mCommonSensorData.readPin, DHT22);
        xSemaphoreGive(dht.mSensorDataMutex);

        vTaskDelay(pdMS_TO_TICKS(1000));
        dhtLib.begin();
        vTaskDelay(pdMS_TO_TICKS(100));

        dht.mHumidity.store(dhtLib.readHumidity());
        dht.mTemperature.store(dhtLib.readTemperature());
        dht.mpLogger->verbosev("Dht22Sensor Read Task", "Humidity: ", (int)(dhtLib.readHumidity()*10));
        dht.mpLogger->verbosev("Dht22Sensor Read Task", "Temperature: ", (int)(dhtLib.readTemperature()*10));
        xSemaphoreGive(dht.mReadingCompleteSemaphore);

        vTaskDelete(nullptr);
    }
}
