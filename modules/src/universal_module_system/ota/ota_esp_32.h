#pragma once

#ifndef ESP32_BOARD
#error "OtaESP32 class is exclusively for ESP32"
#endif

#include <string_view>
#include <memory>
#include <atomic>
#include <WiFi.h>

#include <nlohmann/json.hpp>

#include "i_ota.h"
#include "utils/logger.h"
#include "universal_module_system/debug_led.h"

namespace nl = nlohmann;

#if not defined(WIFI_SSID) or not defined(WIFI_PASS)
#error "Add Wi-Fi SSID and password to /config/user_config/.env file."
#endif

namespace UniversalModuleSystem {
    /**
     * @brief Singleton class implementing an ESP32-specific OTA manager.
     * @details Provides ArduinoOTA handling on ESP32. It is responsible for connecting to Wi-Fi,
     * starting an OTA handling task, and cleaning up OTA resources when OTA mode ends.
     */
    class OtaESP32 final : public IOta {
    public:
        /**
         * @brief Get the singleton instance of OtaESP32.
         *
         * @param logger Shared pointer to the logger instance, default: nullptr.
         * @param debugLED Shared pointer to the DebugLED instance, default: nullptr.
         * @return Reference to the singleton instance.
         *
         * @warning The first call has to pass pointers to logger and debugLED.
         */
        static OtaESP32 &getInstance(
            const std::shared_ptr<ul::Logger> &logger = nullptr,
            const std::shared_ptr<DebugLED> &debugLED = nullptr
        );

        // Delete copy constructor and assignment operator
        OtaESP32(const OtaESP32 &) = delete;

        OtaESP32 &operator =(const OtaESP32 &) = delete;

        /**
         * @brief Start OTA mode.
         * @details Connects to Wi-Fi (if not already connected), stores the assigned
         * IPv4 address, and starts a dedicated FreeRTOS task that handles ArduinoOTA.
         *
         * @return Array containing the device IPv4 address.
         *
         * @note If OTA is already started, returns the previously stored IP address.
         */
        std::array<uint8_t, s_IP_ADDRESS_LENGTH> beginOta() override;

        /**
         * @brief Stop OTA mode.
         * @details Deletes the OTA task, disconnects from Wi-Fi and turns off the Wi-Fi modem.
         */
        void endOta() override;

        /**
         * @brief Toggle OTA mode.
         */
        void toggleOta() override;

        /**
         * @brief Checks if the module is connected to Wi-Fi.
         *
         * @return True if the module is connected to Wi-Fi, false otherwise.
         */
        bool isConnectedToWifi() const override;

        /**
         * @brief Auto-start OTA under special conditions and halt the rest of the program.
         * @details Compares version numbers stored in base_config.json and critical_config.h and checks
         * whether the pairing button is pressed. If version numbers do not match or pairing button
         * is pressed, starts OTA and stops execution rest of the program.
         *
         * @note This may be useful for software updates that are not compatible with an old base_config.json.
         */
        void autoBeginOta() override;

    private:
        /**
         * @brief Construct OtaESP32 and create FreeRTOS mutex.
         *
         * @param logger Shared pointer to the logger instance, default: nullptr.
         * @param debugLED Shared pointer to the DebugLED instance, default: nullptr.
         */
        explicit OtaESP32(
            const std::shared_ptr<ul::Logger> &logger = nullptr,
            const std::shared_ptr<DebugLED> &debugLED = nullptr
        );

        /**
         * @brief Clears up FreeRTOS resources.
         */
        ~OtaESP32() override;

        /**
         * @brief Connect to Wi-Fi.
         * @details Stops LED blinking and turns it on to indicate a successful Wi-Fi connection.
         *
         * @return Array containing the device IPv4 address (4 bytes).
         */
        [[nodiscard]] std::array<uint8_t, s_IP_ADDRESS_LENGTH> connectToWifi();

        /**
         * @brief Disconnect from Wi-Fi and turn off the Wi-Fi modem.
         *
         * @details Stops LED blinking and turns it off to indicate disconnection from Wi-Fi.
         */
        void disconnectFromWifi();

        /**
         * @brief Delete the FreeRTOS task and stop ArduinoOTA.
         */
        void deleteOtaTask();

        /**
         * @brief FreeRTOS task for OTA handling.
         * @details Initializes ArduinoOTA, then periodically calls ArduinoOTA.handle().
         * It also prevents the ESP from entering deep sleep due to idle timer.
         *
         * @param parameters Pointer to the OtaESP32 instance (this).
         */
        static void otaTask(void *parameters);

        /**
         * @brief Creates and starts a FreeRTOS software timer used to signal a Wi-Fi connection timeout.
         */
        void createAndStartWifiConnectionTimeoutTimer();

        /**
         * @brief Stops and deletes the FreeRTOS software timer used to signal a Wi-Fi connection timeout.
         */
        void stopAndDeleteWifiConnectionTimeoutTimer();

        /**
         * @brief Wi-Fi connection timeout timer callback, notifies a task about a connection timeout.
         * @param xTimer Timer handle with the task handle that started the connection attempt.
         */
        static void wifiConnectionTimeoutTimerCallback(TimerHandle_t xTimer);


        std::shared_ptr<ul::Logger> mpLogger;
        std::shared_ptr<DebugLED> mpDebugLED;
        std::array<uint8_t, 4> mIpAddress{};
        std::atomic<bool> mIsOtaForced{false};

        // FreeRTOS
        TaskHandle_t mOtaTaskHandle = nullptr;
        SemaphoreHandle_t mOtaMutex = nullptr;
        TimerHandle_t mWifiConnectionTimeoutTimer = nullptr;

        // constants
        static constexpr TickType_t ms_WIFI_CONNECTION_TIMEOUT_TIME = pdMS_TO_TICKS(1000*10); // 10s
        static constexpr uint8_t ms_MAX_CONNECTION_ATTEMPTS_WITH_FORCED_OTA = 3;

        // JSON key
        static constexpr std::string_view ms_OTA_DATA = "otaCheck";
    };
}
