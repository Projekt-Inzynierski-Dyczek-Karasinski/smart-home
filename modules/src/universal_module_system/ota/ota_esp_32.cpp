#include "ota_esp_32.h"

#include <esp_wifi.h>

#include "universal_module_system/data_manager.h"
#include "universal_module_system/pairing_button.h"
#include "universal_module_system/power_manager/power_manager.h"

namespace UniversalModuleSystem {
    OtaESP32 &OtaESP32::getInstance(const std::shared_ptr<ul::Logger> &logger) {
        static OtaESP32 instance(logger);
        return instance;
    }
    
    OtaESP32::OtaESP32(const std::shared_ptr<ul::Logger> &logger) :
        mOtaMutex(xSemaphoreCreateMutex()),
        mpLogger(logger) {
        if (logger == nullptr) {
            mpLogger = std::make_shared<ul::Logger>();
            mpLogger->error("OtaESP32", "OtaESP32's constructor didn't get pointer to logger instance.");
        }
        mpLogger->verbose("OtaESP32", "OtaESP32 initialized.");
    }

    OtaESP32::~OtaESP32() {
        endOta();
        if (mOtaMutex != nullptr) {
            vSemaphoreDelete(mOtaMutex);
        }
    }

    std::array<uint8_t, IOta::s_IP_ADDRESS_LENGTH> OtaESP32::beginOta() {
        xSemaphoreTake(mOtaMutex, portMAX_DELAY);
        // if ota already begun (ip address is set) just return ip address
        if (!(
            ipAddress[0] == 0 &&
            ipAddress[1] == 0 &&
            ipAddress[2] == 0 &&
            ipAddress[3] == 0
        )) {
            const std::array<uint8_t, s_IP_ADDRESS_LENGTH> result = ipAddress;
            xSemaphoreGive(mOtaMutex);
            return result;
        }

        const std::array<uint8_t, s_IP_ADDRESS_LENGTH> newIpAddress = connectToWifi();
        ipAddress = newIpAddress;
        xSemaphoreGive(mOtaMutex);

        if (mOtaTaskHandle == nullptr) {
            xTaskCreate(
                otaTask,
                "OTA Task",
                OTA_TASK_SIZE,
                this,
                HIGH_TASK_PRIORITY,
                &mOtaTaskHandle
            );
        }
        return newIpAddress;
    }

    void OtaESP32::endOta() {
        deleteOtaTask();
        disconnectFromWifi();
    }

    void OtaESP32::toggleOta() {
        bool isOtaBegan = true;
        xSemaphoreTake(mOtaMutex, portMAX_DELAY);
        if (
            ipAddress[0] == 0 &&
            ipAddress[1] == 0 &&
            ipAddress[2] == 0 &&
            ipAddress[3] == 0
        ) {
            isOtaBegan = false;
        }
        xSemaphoreGive(mOtaMutex);

        if (isOtaBegan) {
            endOta();
        } else {
            beginOta();
        }
    }

    void OtaESP32::autoBeginOta() {
        const auto &dataManager = DataManager::getInstance();
        nl::json otaData = dataManager.loadJson(dataManager.s_BASE_CONFIG_PATH);
        const uint8_t otaCheck = otaData[ms_OTA_DATA][ms_OTA_VERSION_CHECK].get<uint8_t>();

        /*
         if magic numbers in ota_config.h and base_config.json don't mach
         or
         PairingButton is pressed
         on system boot, begin ota and stop rest of the program
        */
        if (otaCheck != OTA_CHECK || PairingButton::isButtonPressed()) {
            mpLogger->warning("OtaESP32", "Ota magic numbers don't mach or ParringButton is pressed.");
            mpLogger->warning("OtaESP32", "Starting OTA and stopping rest of the program.");
            beginOta();

            for (;;) vTaskDelay(1000);
        }
    }

    std::array<uint8_t, IOta::s_IP_ADDRESS_LENGTH> OtaESP32::connectToWifi() const {
        WiFi.begin(WIFI_SSID, WIFI_PASS);
        while (WiFi.status() != WL_CONNECTED) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        mpLogger->info("OtaESP32 IP", WiFi.localIP().toString().c_str());

        IPAddress ip = WiFi.localIP();
        const std::array<uint8_t, s_IP_ADDRESS_LENGTH> result = { ip[0], ip[1], ip[2], ip[3] };

        return result;
    }

    void OtaESP32::disconnectFromWifi() {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        esp_wifi_stop();

        xSemaphoreTake(mOtaMutex, portMAX_DELAY);
        ipAddress = std::array<uint8_t, s_IP_ADDRESS_LENGTH> {0, 0, 0, 0};
        xSemaphoreGive(mOtaMutex);
    }

    void OtaESP32::deleteOtaTask() {
        if (mOtaTaskHandle != nullptr) {
            vTaskDelete(mOtaTaskHandle);
            mOtaTaskHandle = nullptr;
            ArduinoOTA.end();
        }
    }

    void OtaESP32::otaTask(void *parameters) {
        ArduinoOTA.begin();
        auto &powerManager = PowerManager::getInstance();

        for (;;) {
            ArduinoOTA.handle();
            powerManager.restartIdleTimer();
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}
