#include "bme_280.h"

#include <cfloat>

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

namespace UniversalModuleSystem::Transducers {
    BME280::BME280(const std::shared_ptr<ul::Logger> &logger) : Sensor(logger) {}

    void BME280::waitUntilReadingEnds() {
        xSemaphoreTake(mReadingCompleteSemaphore, portMAX_DELAY);
        xSemaphoreGive(mReadingCompleteSemaphore);

        // if reading was newer started
        if (mHumidity.load() == 0 && mTemperature.load() == 0 && mPressure.load() == 0) {
            startReading();
            xSemaphoreTake(mReadingCompleteSemaphore, portMAX_DELAY);
            xSemaphoreGive(mReadingCompleteSemaphore);
        }
    }

    std::vector<API::APIParameterVariant> BME280::getApiFormattedReading() {
        waitUntilReadingEnds();

        // if sensor is not responding
        if (mHumidity.load() == FLT_MAX && mTemperature.load() == 0 && mPressure.load() == 0) {
            return std::vector<API::APIParameterVariant> {
                API::APIParameter((uint8_t)API::errorTypes::INTERNAL_ERROR, true)
            };
        }

        mpLogger->debugv("BME280 reading", "Humidity (%): ", mHumidity.load());
        mpLogger->debugv("BME280 reading", "Temperature (C): ", mTemperature.load());
        mpLogger->debugv("BME280 reading", "Pressure (hPa): ", mPressure.load());

        return std::vector<API::APIParameterVariant> {
            API::APIParameter(mHumidity.load()),
            API::APIParameter(mPressure.load()),
            API::APIParameter(mTemperature.load())
        };
    }

    void BME280::startReading() {
        xSemaphoreTake(mReadingCompleteSemaphore, 0); // make sure that semaphore indicates that reading is not completed
        xTaskCreate(
            bmeReadTask,
            "BME280 Read Task",
            BME280_READ_TASK_SIZE,
            this,
            HIGH_TASK_PRIORITY,
            nullptr
        );
    }

    void BME280::bmeReadTask(void *parameters) {
        auto& bme = *static_cast<BME280*>(parameters);
        vTaskDelay(pdMS_TO_TICKS(100));

        Adafruit_BME280 bme280; // I2C
        TwoWire wire(0);

        xSemaphoreTake(bme.mSensorDataMutex, portMAX_DELAY);
        wire.begin(bme.mCommonSensorData.readPin,bme.mAdditionalData.readPin2);
        const bool isBME280Connected = bme280.begin(bme.mAdditionalData.i2cAddress, &wire);
        xSemaphoreGive(bme.mSensorDataMutex);

        if (!isBME280Connected) {
            bme.mpLogger->error("BME280 Read Task", "BME sensor is not responding.");
            bme.mHumidity.store(FLT_MAX);
            xSemaphoreGive(bme.mReadingCompleteSemaphore);
            vTaskDelete(nullptr);
        }

        const float humidity = bme280.readHumidity();
        const float pressure = bme280.readPressure() / 100.0F;
        const float temperature = bme280.readTemperature();

        bme.mHumidity.store(humidity);
        bme.mPressure.store(pressure);
        bme.mTemperature.store(temperature);
        xSemaphoreGive(bme.mReadingCompleteSemaphore);

        bme.mpLogger->debugv("BME280 Read Task", "Humidity:    ", (int)humidity);
        bme.mpLogger->debugv("BME280 Read Task", "Pressure:    ", (int)pressure);
        bme.mpLogger->debugv("BME280 Read Task", "Temperature: ", (int)temperature);
        wire.end();

        vTaskDelay(pdMS_TO_TICKS(10)); // delay for logger
        vTaskDelete(nullptr);
    }

    bool BME280::loadAdditionalData(const nl::json &jsonData) {
        bool isLoadedSuccessfully = true;
        try {
            mAdditionalData = AdditionalData(jsonData);
        } catch (...) {
            mpLogger->error("BatterySensorESP32 class", "Failed to load additional sensor data.");
            isLoadedSuccessfully = false;
        }
        return isLoadedSuccessfully;
    }

    BME280::AdditionalData::AdditionalData(const nl::json &json) :
        readPin2(json[ms_DATA][ms_SECOND_READ_PIN]),
        i2cAddress(json[ms_DATA][ms_I2C_ADDRESS]),
        isLoaded(true) {}
}
