#pragma once

#include "ota_esp_32.h"
// here include other Ota derived classes

namespace UniversalModuleSystem {
#ifdef ESP32_BOARD
    using Ota = OtaESP32;
    // #else
    // here add alias for other Ota derived classes (and uncomment "#else")
#endif
}
