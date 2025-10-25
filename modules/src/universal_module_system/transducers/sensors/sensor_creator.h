#pragma once

#include <memory>

#include "sensor.h"

namespace UniversalModuleSystem::Transducers {
    class SensorCreator {
    public:
        virtual ~SensorCreator() = default;
        virtual std::shared_ptr<Sensor> FactoryMethod() = 0;

        void handleOnBoot(const std::shared_ptr<Sensor> &sensor) const;
        void handleOnSleep(const std::shared_ptr<Sensor> &sensor) const;

        String handleGetApiFormatedRead(const std::shared_ptr<Sensor> &sensor) const;
    };
}