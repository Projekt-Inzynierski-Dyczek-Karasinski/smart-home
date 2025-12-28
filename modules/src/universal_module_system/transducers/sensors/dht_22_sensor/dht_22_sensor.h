#pragma once

#include "universal_module_system/transducers/sensors/sensor.h"

// TODO !pr add comments

namespace UniversalModuleSystem::Transducers {
    class Dht22Sensor final : public Sensor {
    public:
        explicit Dht22Sensor(const std::shared_ptr<ul::Logger> &logger);

        /**
         * @brief Waits until the sensor reading completes.
         * @details It is used when only waiting for the reading to finish is needed, but not the reading result.
         * <code>getApiFormattedReading</code> automatically waits for the reading to finish before returning the result.
         */
        void waitUntilReadingEnds() override;

        std::vector<API::APIParameterVariant> getApiFormattedReading() override;

        void startReading() override;

    private:
        static void dht22ReadTask(void *parameters);

        std::atomic<float> mHumidity{0};
        std::atomic<float> mTemperature{0};
    };
}
