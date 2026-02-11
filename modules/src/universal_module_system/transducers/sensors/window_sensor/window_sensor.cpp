#include "window_sensor.h"

#include "universal_module_system/power_manager/power_manager.h"

namespace UniversalModuleSystem::Transducers {
    WindowSensor::WindowSensor(const std::shared_ptr<ul::Logger> &logger) : Sensor(logger) {
        xSemaphoreTake(mSensorDataMutex, portMAX_DELAY);
        pinMode(mCommonSensorData.readPin, INPUT_PULLUP);
        xSemaphoreGive(mSensorDataMutex);
    }

    std::vector<API::APIParameterVariant> WindowSensor::getApiFormattedReading() {
        using wss = windowSensorStatus;
        xSemaphoreTake(mSensorDataMutex, portMAX_DELAY);
        const wss windowState = digitalRead(mCommonSensorData.readPin) ? wss::CLOSE : wss::OPEN;
        xSemaphoreGive(mSensorDataMutex);

        return std::vector<API::APIParameterVariant>{API::APIParameter<uint8_t>(static_cast<uint8_t>(windowState))};
    }

    void WindowSensor::onSleep() {
        xSemaphoreTake(mSensorDataMutex, portMAX_DELAY);

        // if waking up ESP32 is disabled in config
        if (!mCommonSensorData.canAwake) {
            xSemaphoreGive(mSensorDataMutex);
            return;
        }

        const bool windowState = digitalRead(mCommonSensorData.readPin);
        mpLogger->debugv("WindowSensor", "windowState: ", windowState);
        const auto &powerManager = PowerManager::getInstance();
        if (!powerManager.addWakeUpOnEXT0(static_cast<gpio_num_t>(mCommonSensorData.readPin), !windowState))
            mpLogger->error("WindowSensor", "Can not assign wake up to WindowSensor.");

        xSemaphoreGive(mSensorDataMutex);
        mpLogger->debug("WindowSensor", "Wake up set on EXT0");
    }

    void WindowSensor::waitUntilReadEnds() {}

    void WindowSensor::startReading() {}
}
