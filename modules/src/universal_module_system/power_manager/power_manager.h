#pragma once

#include "power_manager_esp_32.h"
// here include other power manager derived classes

namespace UniversalModuleSystem {
#ifdef ESP32_BOARD
    using PowerManager = PowerManagerESP32;
    // #else
    // here add alias for other power manager derived classes (and uncomment "#else")
#endif
}
