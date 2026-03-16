#include "window_sensor.h"

#include "universal_module_system/power_manager/power_manager.h"

namespace UniversalModuleSystem::Transducers {
    RTC_DATA_ATTR bool WindowSensor::msIsFirstSleepNeeded = true;

    WindowSensor::WindowSensor(const std::shared_ptr<ul::Logger> &logger) : Sensor(logger) {
        xSemaphoreTake(mSensorDataMutex, portMAX_DELAY);
        pinMode(mCommonSensorData.readPin, INPUT_PULLUP);
        xSemaphoreGive(mSensorDataMutex);

        handleFirstSleep();
    }

    WindowSensor::~WindowSensor() {
        if (mFirstSleepTaskHandle != nullptr) {
            vTaskDelete(mFirstSleepTaskHandle);
            mFirstSleepTaskHandle = nullptr;
        }
    }

    std::vector<API::APIParameterVariant> WindowSensor::getApiFormattedReading() {
        using wss = windowSensorStatus;
        xSemaphoreTake(mSensorDataMutex, portMAX_DELAY);
        const wss windowState = digitalRead(mCommonSensorData.readPin) ? wss::CLOSED : wss::OPEN;
        xSemaphoreGive(mSensorDataMutex);

        mpLogger->debugv("WindowSensor", "Sensor state: ", static_cast<int>(windowState));

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

    void WindowSensor::firstSleepTask(void *parameters) {
        const auto &ws = *static_cast<WindowSensor *>(parameters);

        // wait until ESP32 fully boots
        vTaskDelay(pdMS_TO_TICKS(1000));

        // go to deep sleep - ESP32 will instantly wake up (this is needed for the propper WindowSensor operation)
        ws.mpLogger->info("WindowSensor", "Going to first sleep...");
        auto &powerManager = PowerManager::getInstance();
        powerManager.enterSleep(1000, false);

        for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
    }

    void WindowSensor::handleFirstSleep() {
        if (msIsFirstSleepNeeded) {
            msIsFirstSleepNeeded = false;
            if (mFirstSleepTaskHandle == nullptr) {
                xTaskCreate(
                    firstSleepTask,
                    "First Sleep",
                    WINDOW_SENSOR_TASK_SIZE,
                    this,
                    CRITICAL_TASK_PRIORITY,
                    &mFirstSleepTaskHandle
                );
            }
        }
    }

    void WindowSensor::waitUntilReadEnds() {}

    void WindowSensor::startReading() {}
}
