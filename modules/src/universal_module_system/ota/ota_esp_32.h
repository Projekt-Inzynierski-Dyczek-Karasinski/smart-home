// #pragma once
//
// #ifndef ESP32_BOARD
// #error "OtaESP32 class is exclusively for ESP32"
// #endif
//
// #include <memory>
// // TODO !pr uncomment
// #include <ArduinoOTA.h>
// #include <nlohmann/json.hpp>
// // TODO !o consider changing lib <WiFi.h> to <esp_wifi.h>, because <WiFi.h> uses 445kB of flash memory
// // TODO !pr uncomment
// #include <WiFi.h>
// #include <esp_wifi.h>
//
// #include "i_ota.h"
// #include "utils/logger.h"
//
// namespace nl = nlohmann;
// namespace ul = Utils::Logging;
//
// namespace UniversalModuleSystem {
//     /**
//      * @brief Class responsible for handling Wi-Fi connection and OTA programming specifically for the ESP32.
//      */
//     class OtaESP32 final : public IOta {
//     public:
//         /**
//          * @brief Constructor, initializes OtaESP32 with a shared logger.
//          * @param logger Shared pointer to logger.
//          */
//         explicit OtaESP32(const std::shared_ptr<ul::Logger> &logger);
//
//         /**
//          * @brief Destructor, stops OTA and disconnects from Wi-Fi.
//          */
//         ~OtaESP32() override;
//
//         /**
//          * @brief Connects to Wi-Fi using credentials stored in SPIFFS.
//          * @return An empty string if it fails to get Wi-Fi credentials; otherwise, returns the local IP address.
//          */
//         String connectToWiFi() override;
//
//         /**
//          * @brief Disconnect from Wi-Fi and disables Wi-Fi module.
//          */
//         void disconnectFromWiFi() override;
//
//         /**
//          * @brief Start FreeRTOS task responsible for OTA programming.
//          */
//         void startOta() override;
//
//         /**
//          * @brief Deletes FreeRTOS task responsible for OTA programming and stops OTA.
//          */
//         void stopOta() override;
//
//         /**
//          * @brief Automatically enables OTA if values \n
//          * <code>CONFIG_VERSION</code> stored in <code>/config/ota_config.h</code> and \n
//          * <code>"version"</code> stored in <code>/base_config.json</code> do not match.
//          *
//          * @details Changing one of these variables, enables to first update program,
//          * and after that update <code>/base_config.json</code> (or vice versa).
//          *
//          * @note It is used as soon as possible in main.cpp.
//          *
//          * @warning (Assuming that is called as soon as possible in main.cpp) It starts OTA and stops completely
//          * executing rest of the program (excluding: OTA, Logger and DataManager) to avoid potential crashes.
//          */
//         void autoEnableOta() override;
//
//     private:
//         /**
//          * @brief FreeRTOS task function for handling OTA processes.
//          * @param parameters Task parameters.
//          */
//         static void otaTask(void * parameters);
//
//         /**
//          * @struct WiFiData
//          * @brief Struct that holds Wi-Fi SSID and password.
//          */
//         struct WiFiData {
//             explicit WiFiData(const nl::json &json);
//
//             String ssid;
//             String password;
//
//         private:
//             static constexpr char ms_PLACEHOLDER_SSID[] = "wifi ssid";
//             static constexpr char ms_PLACEHOLDER_PASSWORD[] = "wifi password";
//
//             // JSON keys
//             static constexpr char ms_SSID[] = "ssid";
//             static constexpr char ms_PASSWORD[] = "password";
//         };
//
//         std::shared_ptr<ul::Logger> mpLogger;
//
//         TaskHandle_t mOtaTaskHandle = nullptr;
//
//         // JSON keys
//         static constexpr char ms_WIFI_PATH[] = "/root/wifi";
//         static constexpr char ms_CONFIG_PATH[] = "/base_config.json";
//         static constexpr char ms_OTA_DATA[] = "ota";
//         static constexpr char ms_VERSION[] = "version";
//         static constexpr char ms_ENABLE_OTA_PIN[] = "ota_pin";
//     };
// }