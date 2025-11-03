#pragma once

#include <Arduino.h>

#include "../config/ota_config.h"

namespace UniversalModuleSystem {
    /**
     * @brief Interface defining methods for Wi-Fi connection and OTA update management.
     *
     * @details Classes implementing this interface must provide functionality to connect and disconnect
     * from Wi-Fi, start and stop OTA updates, and automatically enable OTA update based on configuration.
     */
    class IOta {
    public:
        virtual ~IOta() = default;

        virtual String connectToWiFi() = 0;
        virtual void disconnectFromWiFi() = 0;

        virtual void startOta() = 0;
        virtual void stopOta() = 0;

        virtual void autoEnableOta() = 0;
    };
}