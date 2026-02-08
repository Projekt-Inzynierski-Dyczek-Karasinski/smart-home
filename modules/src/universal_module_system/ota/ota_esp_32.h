#pragma once

#ifndef ESP32_BOARD
    #error "OtaESP32 class is exclusively for ESP32"
#endif

#include <string_view>
#include <memory>
#include <WiFi.h>

#include <nlohmann/json.hpp>

#include "i_ota.h"
#include "utils/logger.h"
#include "universal_module_system/debug_led.h"

namespace nl = nlohmann;

namespace UniversalModuleSystem {
    class OtaESP32 final : public IOta {
    public:
        static OtaESP32& getInstance(
            const std::shared_ptr<ul::Logger> &logger = nullptr,
            const std::shared_ptr<DebugLED> &debugLED = nullptr
        );

        // Delete copy constructor and assignment operator
        OtaESP32(const OtaESP32&) = delete;
        OtaESP32& operator = (const OtaESP32&) = delete;

        std::array<uint8_t, s_IP_ADDRESS_LENGTH> beginOta() override;

        void endOta() override;

        void toggleOta() override;

        void autoBeginOta() override;

    private:
        explicit OtaESP32(
            const std::shared_ptr<ul::Logger> &logger = nullptr,
            const std::shared_ptr<DebugLED> &debugLED = nullptr
        );

        ~OtaESP32() override;

        [[nodiscard]] std::array<uint8_t, s_IP_ADDRESS_LENGTH> connectToWifi() const;

        void disconnectFromWifi();

        void deleteOtaTask();

        static void otaTask(void * parameters);

        TaskHandle_t mOtaTaskHandle = nullptr;
        SemaphoreHandle_t mOtaMutex = nullptr;

        std::shared_ptr<ul::Logger> mpLogger;
        std::shared_ptr<DebugLED> mpDebugLED;
        std::array<uint8_t, 4> ipAddress{};

        // JSON keys
        static constexpr std::string_view ms_OTA_DATA = "otaCheck";
    };
}
