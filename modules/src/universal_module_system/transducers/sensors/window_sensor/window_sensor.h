#pragma once

#include "universal_module_system/transducers/sensors/sensor.h"

// TODO !pr add comments
namespace UniversalModuleSystem::Transducers {
    enum class windowSensorStatus: uint8_t {
        CLOSE = 0,
        OPEN = 1,
    };

    class WindowSensor final : public Sensor {
    public:
        explicit WindowSensor(const std::shared_ptr<ul::Logger> &logger);

        std::vector<API::APIParameterVariant> getApiFormattedReading() override;

        void onSleep() override;

        void startReading() override;

        void waitUntilReadEnds() override;
    };
}
