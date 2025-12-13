// // TODO !pr uncomment
//
// #include "ota_esp_32.h"
//
// #include "universal_module_system/data_manager.h"
//
// namespace UniversalModuleSystem {
//     OtaESP32::OtaESP32(const std::shared_ptr<ul::Logger> &logger) : mpLogger(logger) {}
//
//     OtaESP32::~OtaESP32() {
//         stopOta();
//         disconnectFromWiFi();
//     }
//
//     String OtaESP32::connectToWiFi() {
//         const auto &dataManager = DataManager::getInstance();
//         const nl::json jsonData = dataManager.loadJson(WIFI_DATA_PATH);
//         if (jsonData.empty()) {
//             mpLogger->error("OtaESP32 connectToWiFi", "Failed to load wifi data.");
//             return String("");
//         }
//
//         const WiFiData wifiData(jsonData);
//         WiFi.begin(wifiData.ssid, wifiData.password);
//         while (WiFi.status() != WL_CONNECTED) {
//             vTaskDelay(pdMS_TO_TICKS(100));
//         }
//
//         mpLogger->info("OtaESP32 IP", WiFi.localIP().toString().c_str());
//
//         return WiFi.localIP().toString();
//     }
//
//     void OtaESP32::disconnectFromWiFi() {
//         WiFi.disconnect(true);
//         WiFi.mode(WIFI_OFF);
//         esp_wifi_stop();
//     }
//
//     void OtaESP32::startOta() {
//         xTaskCreate(
//             otaTask,
//             "OTA Task",
//             OTA_TASK_SIZE,
//             this,
//             BACKGROUND_TASK_PRIORITY,
//             &mOtaTaskHandle
//         );
//     }
//
//     void OtaESP32::stopOta() {
//         vTaskDelete(mOtaTaskHandle);
//         ArduinoOTA.end();
//     }
//
//     void OtaESP32::autoEnableOta() {
//         const auto &dataManager = DataManager::getInstance();
//         const nl::json baseConfig = dataManager.loadJson(ms_CONFIG_PATH);
//         if (!baseConfig.contains(ms_OTA_DATA) || !baseConfig[ms_OTA_DATA].contains(ms_VERSION)) {
//             mpLogger->error("OtaESP32", "Failed to load Ota data from /base_config.json.");
//             return;
//         }
//
//         const nl::json & jsonData = baseConfig[ms_OTA_DATA];
//
//         bool isVersionIncorrect = false;
//         bool isOtaForced = false;
//         if (CONFIG_VERSION != jsonData[ms_VERSION].get<uint8_t>()) {
//             isVersionIncorrect = true;
//         } else if (jsonData.contains(ms_ENABLE_OTA_PIN) && jsonData[ms_ENABLE_OTA_PIN] != 0) {
//             const uint8_t otaPin = jsonData[ms_ENABLE_OTA_PIN].get<uint8_t>();
//             pinMode(otaPin, INPUT_PULLUP);
//             vTaskDelay(pdMS_TO_TICKS(5)); // give time for pin to set
//             isOtaForced = digitalRead(otaPin) == LOW;
//         }
//
//         if (isVersionIncorrect || isOtaForced) {
//             if (isVersionIncorrect) {
//                 mpLogger->error(
//                     "OtaESP32",
//                     "Config versions in /config/ota_config.h and /data/base_config.json do not match."
//                 );
//                 mpLogger->error(
//                     "OtaESP32",
//                     "Starting Ota and stopping rest of program..."
//                 );
//             } else {
//                 mpLogger->warningv(
//                     "OtaESP32",
//                     "Ota is forced by GPIO: ",
//                     jsonData[ms_ENABLE_OTA_PIN].get<uint8_t>()
//                 );
//                 mpLogger->error(
//                     "OtaESP32",
//                     "Starting Ota and stopping rest of program..."
//                 );
//             }
//             startOta();
//
//             for (;;) {
//                 vTaskDelay(pdMS_TO_TICKS(1000));
//             }
//         }
//     }
//
//     void OtaESP32::otaTask(void * parameters) {
//         auto &ota = *static_cast<OtaESP32*>(parameters);
//         if (WiFi.status() != WL_CONNECTED) {
//             ota.connectToWiFi();
//         }
//
//         ArduinoOTA.begin();
//
//         for (;;) {
//             ArduinoOTA.handle();
//             vTaskDelay(pdMS_TO_TICKS(5));
//         }
//     }
//
//     OtaESP32::WiFiData::WiFiData(const nl::json &json) :
//         ssid(json[ms_SSID].get<std::string>().data()),
//         password(json[ms_PASSWORD].get<std::string>().data()) {
//         if (ssid == ms_PLACEHOLDER_SSID && password == ms_PLACEHOLDER_PASSWORD) {
//             ul::Logger logger;
//             logger.warning("OtaESP32 WiFiData", "WiFi ssid and password are identical to placeholders, did you forget to change them?");
//         }
//     }
// }