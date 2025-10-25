#pragma once

#ifndef ESP32_BOARD
#error "BatterySensorESP32 class is exclusively for ESP32"
#endif

#include "i_battery_sensor.h"

namespace UniversalModuleSystem::Transducers {
    class BatterySensorESP32 final : public IBatterySensor {
    };
}
