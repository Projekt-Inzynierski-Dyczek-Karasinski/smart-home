#pragma once

#include "universal_module_system/transducers/sensors/sensor.h"

namespace UniversalModuleSystem::Transducers {
    class Dht22Sensor final : public Sensor {
    public:
        explicit Dht22Sensor(const std::shared_ptr<ul::Logger> &logger);

        uint32_t getReading() override;

        void startReading() override;

    private:
        static void dht22ReadTask(void *parameters);

        std::atomic<float> mHumidity{0};
        std::atomic<float> mTemperature{0};
    };
}
