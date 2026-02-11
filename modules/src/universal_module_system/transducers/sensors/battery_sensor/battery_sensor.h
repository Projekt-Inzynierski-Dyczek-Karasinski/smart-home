#pragma once

#include "battery_sensor_esp_32.h"
// here include other power manager derived classes

namespace UniversalModuleSystem::Transducers {
#ifdef ESP32_BOARD
    using BatterySensor = BatterySensorESP32;
    // #else
    // here add alias for other battery sensor derived classes (and uncomment "#else")
#endif
}
