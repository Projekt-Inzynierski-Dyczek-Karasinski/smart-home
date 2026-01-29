#pragma once

#include "universal_module_system/transducers/actuators/actuator.h"
#include <atomic>

namespace UniversalModuleSystem::Transducers {
    enum class relayOperations : uint8_t {
        TURN_OFF = 0,
        TURN_ON = 1,
    };

    class Relay final : public Actuator {
    public:
        explicit Relay(const std::shared_ptr<ul::Logger> &logger);

        apiPv getState() override;

        apiPv toggle() override;

        apiPv doOperation(apiPv operation) override;

        void onBoot(const nl::json &jsonData) override;

    private:
        actuatorState getStatePrivate() const;

        void changePinOutput(bool state);

        static std::atomic<uint32_t> relayStateBitmask;
    };
}