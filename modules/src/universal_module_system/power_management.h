#pragma once

// functions in namespace PowerManagement are exclusively for ESP32
#ifndef ESP32_BOARD
#error "Power management not implemented"
#endif

#include <Arduino.h>

namespace UniversalModuleSystem {
    namespace PowerManagement {
        void waitAndDisableCriticalFeatures();

        void enterSleep(uint32_t seconds);

        void safeRestart(const char *source);

        // TODO !BEFORE PULL REQUEST! remove
        void printWakeUpReason();
    }
}
