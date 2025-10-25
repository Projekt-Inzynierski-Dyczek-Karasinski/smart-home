#include "sensor.h"

#include "universal_module_system/data_manager.h"

namespace UniversalModuleSystem::Transducers {
    Sensor::Sensor(const std::shared_ptr<ul::Logger> &logger, String dataPath)
    : mpLogger(logger), mDataPath(std::move(dataPath)), mSensorDataMutex(xSemaphoreCreateMutex()) {}

    Sensor::~Sensor() {
        vSemaphoreDelete(mSensorDataMutex);
    }

    uint8_t Sensor::getId() {
        xSemaphoreTake(mSensorDataMutex, portMAX_DELAY);
        if (!mSensorData.has_value()) {
            xSemaphoreGive(mSensorDataMutex);
            mpLogger->error("Sensor class", "Can't get sensor id, sensor data is not loaded.");
            return 0; // TODO consider throwing error
        }
        const uint8_t result = mSensorData.value().id;
        xSemaphoreGive(mSensorDataMutex);

        return result;
    }

    void Sensor::onBoot() {
        loadData();
        read();
    }

    void Sensor::loadData() {
        const auto &dataManager = DataManager::getInstance();
        const nl::json jsonData = dataManager.loadJson(mDataPath.c_str());

        xSemaphoreTake(mSensorDataMutex, portMAX_DELAY);
        try {
            mSensorData = SensorData(jsonData);
        } catch (...) {
            mpLogger->error("Sensor class", "Failed to load sensor data.");
            mSensorData.reset();
        }
        loadAdditionalData(jsonData);
        xSemaphoreGive(mSensorDataMutex);
    }

    Sensor::SensorData::SensorData(const nl::json &json)
        : id(json[ms_ID]), readPin(json[ms_READ_PIN]), canAwake(json[ms_CAN_AWAKE]) {
        if (json[ms_POWER_PIN] != 0) {
            powerPin = json[ms_POWER_PIN];
        }
    }
}