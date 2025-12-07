#include "sensor.h"

#include "universal_module_system/data_manager.h"

namespace UniversalModuleSystem::Transducers {
    Sensor::Sensor(const std::shared_ptr<ul::Logger> &logger)
        : mpLogger(logger),
          mSensorDataMutex(xSemaphoreCreateMutex()),
          mReadingCompleteSemaphore(xSemaphoreCreateBinary()) {
        //TODO !pr remove
        mpLogger = std::make_shared<ul::Logger>(ul::Level::VERBOSE);
    }

    Sensor::~Sensor() {
        vSemaphoreDelete(mSensorDataMutex);
        vSemaphoreDelete(mReadingCompleteSemaphore);
    }

    String Sensor::getApiFormattedReading() {
        String result = String(getId());
        result.concat(':');
        result.concat(getReading());
        return result;
    }

    uint8_t Sensor::getId() const {
        xSemaphoreTake(mSensorDataMutex, portMAX_DELAY);
        const uint8_t result = mCommonSensorData.id;
        xSemaphoreGive(mSensorDataMutex);
        return result;
    }

    bool Sensor::init(const nl::json &jsonData) {
        return loadData(jsonData);
    }

    bool Sensor::loadData(const nl::json &jsonData) {
        bool isLoadedSuccessfully = true;
        xSemaphoreTake(mSensorDataMutex, portMAX_DELAY);
        try {
            mCommonSensorData = CommonSensorData(jsonData);
        } catch (...) {
            mpLogger->error("Sensor class", "Failed to load common sensor data.");
            isLoadedSuccessfully = false;
        }
        if (isLoadedSuccessfully) {
            isLoadedSuccessfully = loadAdditionalData(jsonData);
        }
        xSemaphoreGive(mSensorDataMutex);
        return isLoadedSuccessfully;
    }

    bool Sensor::loadAdditionalData(const nl::json& jsonData) {
        return true;
    }

    Sensor::CommonSensorData::CommonSensorData(const nl::json &json) :
        id(json[ms_ID]),
        readPin(json[ms_READ_PIN]),
        canAwake(json[ms_CAN_AWAKE]),
        isLoaded(true) {}
}