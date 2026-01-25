#include "relay.h"

namespace UniversalModuleSystem::Transducers {
    using ET = API::errorTypes;
    // FIXME: software reset (not wake up from deep sleep, not reset by en pin, only software reset) deletes RTC_DATA_ATTR memory, but not clears gpio_hold

    RTC_DATA_ATTR std::atomic<uint32_t> Relay::relayStateBitmask = 0; // by default every relay is off

    Relay::Relay(const std::shared_ptr<ul::Logger> &logger) : Actuator(logger) {}

    apiPv Relay::getState() {
        std::optional<apiPv> result;
        try {
            result.emplace(apiPv{API::APIParameter(static_cast<uint8_t>(getStatePrivate()))}) ;
        } catch (std::exception &e) {
            mpLogger->error("Relay", e.what());
            result.emplace(apiPv{API::APIParameter(static_cast<uint8_t>(ET::INTERNAL_ERROR), true)});
        }
        return result.value();
    }

    apiPv Relay::toggle() {
        actuatorState currentState;
        // get relay current state
        try {
            currentState = getStatePrivate();
        } catch (std::exception &e) {
            mpLogger->error("Relay", e.what());
            return  apiPv{API::APIParameter(static_cast<uint8_t>(ET::INTERNAL_ERROR), true)};
        }

        if (currentState == actuatorState::OFF) {
            changePinOutput(HIGH);
            return apiPv{API::APIParameter(static_cast<uint8_t>(actuatorState::ON))};
        } else {
            changePinOutput(LOW);
            return apiPv{API::APIParameter(static_cast<uint8_t>(actuatorState::OFF))};
        }
    }

    apiPv Relay::doOperation(apiPv operation) {
        actuatorState currentState;
        uint8_t operationValue;
        // get relay current state
        try {
            currentState = getStatePrivate();
        } catch (std::exception &e) {
            mpLogger->error("Relay", e.what());
            return  apiPv{API::APIParameter(static_cast<uint8_t>(ET::INTERNAL_ERROR), true)};
        }

        // get value of passed operation
        try {
            operationValue = std::get<API::APIParameter<uint8_t>>(operation).getValue();
        } catch (std::exception &e) {
            mpLogger->error("Relay", e.what());
            return apiPv{API::APIParameter(static_cast<uint8_t>(ET::BAD_ARGUMENT), true)};
        }

        // do operation
        using RO = relayOperations;
        switch (operationValue) {
            case static_cast<uint8_t>(RO::TURN_ON):
                if (currentState != actuatorState::ON)
                    changePinOutput(HIGH);
                return apiPv{API::APIParameter(operationValue)};

            case static_cast<uint8_t>(RO::TURN_OFF):
                if (currentState != actuatorState::OFF)
                    changePinOutput(LOW);
                return apiPv{API::APIParameter(operationValue)};

            default:
                mpLogger->error("Relay", "Relay does not support this operation.");
                return apiPv{API::APIParameter(static_cast<uint8_t>(ET::BAD_ARGUMENT), true)};
        }
    }

    actuatorState Relay::getStatePrivate() const {
        // bitmask "overflow" protection
        const uint8_t id = getId();
        if (id >= sizeof(relayStateBitmask)) {
            mpLogger->errorv("Relay RTC_DATA_ATTR", "Relay id is bigger that size of relayStateBitmask, relay id: ", id);
            throw std::invalid_argument("Relay id is bigger that size of relayStateBitmask.");
        }

        // get state from bitmask based on relay id
        return (relayStateBitmask.load() >> id & 1UL) == 1 ? actuatorState::ON : actuatorState::OFF;
    }

    void Relay::changePinOutput(const bool state) {
        xSemaphoreTake(mActuatorDataMutex, portMAX_DELAY);

        // change pin state with holding it in ESP32 deep sleep
        gpio_deep_sleep_hold_dis();
        gpio_hold_dis(static_cast<gpio_num_t>(mCommonActuatorData.logicPin));

        pinMode(mCommonActuatorData.logicPin, OUTPUT);
        digitalWrite(mCommonActuatorData.logicPin, state);

        gpio_hold_en(static_cast<gpio_num_t>(mCommonActuatorData.logicPin));
        gpio_deep_sleep_hold_en();

        // change bitmask
        // id should be already checked, no bitmask "overflow" protection
        if (state)
            relayStateBitmask |=  (1UL << mCommonActuatorData.id);
        else
            relayStateBitmask &= ~(1UL << mCommonActuatorData.id);

        xSemaphoreGive(mActuatorDataMutex);
    }
}
