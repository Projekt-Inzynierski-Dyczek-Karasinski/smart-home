#include "sensor_creator.h"

namespace UniversalModuleSystem::Transducers {
    void SensorCreator::handleOnBoot(const std::shared_ptr<Sensor> &sensor) const {
        sensor->onBoot();
    }

    void SensorCreator::handleOnSleep(const std::shared_ptr<Sensor> &sensor) const {
        sensor->onSleep();
    }

    String SensorCreator::handleGetApiFormatedRead(const std::shared_ptr<Sensor> &sensor) const {
        return sensor->getApiFormatedRead();
    }
}
