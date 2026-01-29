#include "actuator.h"

namespace UniversalModuleSystem::Transducers {
    using ET = API::errorTypes;

    Actuator::Actuator(const std::shared_ptr<ul::Logger> &logger)
        : mpLogger(logger),
          mActuatorDataMutex(xSemaphoreCreateMutex()) {}

    Actuator::~Actuator() {
        vSemaphoreDelete(mActuatorDataMutex);
    }

    apiPv Actuator::toggle() {
        return API::APIParameterVariant{API::APIParameter(static_cast<uint8_t>(ET::BAD_ARGUMENT), true)};
    }
    
    apiPv Actuator::getState() {
        return API::APIParameterVariant{API::APIParameter(static_cast<uint8_t>(actuatorState::UNKNOWN), true)};
    }

    uint8_t Actuator::getId() const {
        xSemaphoreTake(mActuatorDataMutex, portMAX_DELAY);
        const uint8_t result = mCommonActuatorData.id;
        xSemaphoreGive(mActuatorDataMutex);
        return result;
    }

    bool Actuator::init(const nl::json &jsonData) {
        return loadData(jsonData);
    }

    void Actuator::onBoot(const nl::json &jsonData) {}

    bool Actuator::loadData(const nl::json &jsonData) {
        bool isLoadedSuccessfully = true;
        xSemaphoreTake(mActuatorDataMutex, portMAX_DELAY);
        try {
            mCommonActuatorData = CommonActuatorData(jsonData);
        } catch (...) {
            mpLogger->error("Sensor class", "Failed to load common sensor data.");
            isLoadedSuccessfully = false;
        }
        if (isLoadedSuccessfully) {
            isLoadedSuccessfully = loadAdditionalData(jsonData);
        }
        xSemaphoreGive(mActuatorDataMutex);
        return isLoadedSuccessfully;
    }
    
    bool Actuator::loadAdditionalData(const nl::json& jsonData) {
        return true;
    }
    
    Actuator::CommonActuatorData::CommonActuatorData(const nl::json &json) :
       id(json[ms_ID]),
       logicPin(json[ms_LOGIC_PIN]),
       isLoaded(true) {}
}
