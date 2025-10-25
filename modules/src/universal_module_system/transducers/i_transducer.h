#pragma once

#include <Arduino.h>

// TODO !pr add comments
namespace UniversalModuleSystem::Transducers {
    class ITransducer {
    public:
        virtual ~ITransducer() = default;

        virtual uint8_t getId() = 0;
        virtual void onBoot() = 0;
        virtual void onSleep() = 0;
    protected:
        virtual void loadData() = 0;
    };
}

