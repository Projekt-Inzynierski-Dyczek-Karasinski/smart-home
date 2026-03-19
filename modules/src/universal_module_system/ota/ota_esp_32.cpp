#include "ota_esp_32.h"

#include <esp_wifi.h>
#include <ArduinoOTA.h>

#include "universal_module_system/data_manager.h"
#include "universal_module_system/pairing_button.h"
#include "universal_module_system/power_manager/power_manager.h"

namespace UniversalModuleSystem {
    OtaESP32 &OtaESP32::getInstance(
        const std::shared_ptr<ul::Logger> &logger,
        const std::shared_ptr<DebugLED> &debugLED
    ) {
        static OtaESP32 instance(logger, debugLED);
        return instance;
    }

    OtaESP32::OtaESP32(
        const std::shared_ptr<ul::Logger> &logger,
        const std::shared_ptr<DebugLED> &debugLED
    ) : mpLogger(logger),
        mpDebugLED(debugLED),
        mOtaMutex(xSemaphoreCreateMutex()) {
        if (logger == nullptr) {
            mpLogger = std::make_shared<ul::Logger>();
            mpLogger->error("OtaESP32", "OtaESP32's constructor didn't get pointer to logger instance.");
        }
        if (debugLED == nullptr) {
            mpDebugLED = std::make_shared<DebugLED>(logger);
            mpLogger->error("OtaESP32", "OtaESP32's constructor didn't get pointer to debugLED instance.");
        }

        mpLogger->verbose("OtaESP32", "OtaESP32 initialized.");
    }

    OtaESP32::~OtaESP32() {
        endOta();

        if (mOtaMutex != nullptr)
            vSemaphoreDelete(mOtaMutex);
    }

    std::array<uint8_t, IOta::s_IP_ADDRESS_LENGTH> OtaESP32::beginOta() {
        xSemaphoreTake(mOtaMutex, portMAX_DELAY);
        // if ota already begun (ip address is set) just return ip address
        if (!(
            mIpAddress[0] == 0 &&
            mIpAddress[1] == 0 &&
            mIpAddress[2] == 0 &&
            mIpAddress[3] == 0
        )) {
            const std::array<uint8_t, s_IP_ADDRESS_LENGTH> result = mIpAddress;
            xSemaphoreGive(mOtaMutex);
            return result;
        }

        const std::array<uint8_t, s_IP_ADDRESS_LENGTH> newIpAddress = connectToWifi();
        mIpAddress = newIpAddress;
        xSemaphoreGive(mOtaMutex);

        if (!isConnectedToWifi()) return newIpAddress;

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
        stopAndDeleteWifiConnectionTimeoutTimer();
        deleteOtaTask();
        disconnectFromWifi();
    }

    void OtaESP32::toggleOta() {
        if (isConnectedToWifi()) {
            endOta();
        } else {
            beginOta();
        }
    }

    bool OtaESP32::isConnectedToWifi() const {
        bool result = true;
        xSemaphoreTake(mOtaMutex, portMAX_DELAY);
        if (
            mIpAddress[0] == 0 &&
            mIpAddress[1] == 0 &&
            mIpAddress[2] == 0 &&
            mIpAddress[3] == 0
        ) {
            result = false;
        }
        xSemaphoreGive(mOtaMutex);
        return result;
    }

    void OtaESP32::autoBeginOta() {
        const auto &dataManager = DataManager::getInstance();
        nl::json otaData = dataManager.loadJson(dataManager.s_BASE_CONFIG_PATH);
        const uint8_t otaCheck = otaData[ms_OTA_DATA].get<uint8_t>();

        /*
         If version numbers in ota_config.h and base_config.json don't mach
         or
         PairingButton is pressed
         on system boot, begin ota and stop rest of the program.
        */
        if (otaCheck != CONFIG_COMPAT_VERSION || PairingButton::isButtonPressed()) {
            mIsOtaForced.store(true);

            mpLogger->warning("OtaESP32", "Ota version numbers don't mach or ParingButton is pressed.");
            mpLogger->warning("OtaESP32", "Starting OTA and stopping rest of the program.");
            beginOta();

            for (;;) vTaskDelay(1000);
        }
    }

    std::array<uint8_t, IOta::s_IP_ADDRESS_LENGTH> OtaESP32::connectToWifi() {
        mpDebugLED->createPairingBlinkTask();
        WiFi.begin(WIFI_SSID, WIFI_PASS);

        createAndStartWifiConnectionTimeoutTimer();
        uint8_t attemptsCounter = 0;
        while (WiFi.status() != WL_CONNECTED) {
            vTaskDelay(pdMS_TO_TICKS(10));
            if (ulTaskNotifyTake(pdTRUE, 0) > 0) {
                mpLogger->error("OtaESP32", "Module has problems with connecting to WiFi...");
                // if ota is started by autoBeginOta()
                if (mIsOtaForced.load()) {
                    if (++attemptsCounter >= ms_MAX_CONNECTION_ATTEMPTS_WITH_FORCED_OTA) {
                        mpLogger->error("OtaESP32", "Connection to WiFi failed. Rebooting...");
                        esp_restart();
                    }
                } else {
                    stopAndDeleteWifiConnectionTimeoutTimer();
                    disconnectFromWifi();
                    return std::array<uint8_t, IOta::s_IP_ADDRESS_LENGTH>{0, 0, 0, 0};
                }
            }
        }
        stopAndDeleteWifiConnectionTimeoutTimer();

        mpLogger->info("OtaESP32 IP", WiFi.localIP().toString().c_str());
        mpDebugLED->wifiConnected();

        IPAddress ip = WiFi.localIP();
        const std::array<uint8_t, s_IP_ADDRESS_LENGTH> result = {ip[0], ip[1], ip[2], ip[3]};

        return result;
    }

    void OtaESP32::disconnectFromWifi() {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        esp_wifi_stop();
        mpDebugLED->wifiDisconnected();

        xSemaphoreTake(mOtaMutex, portMAX_DELAY);
        mIpAddress = std::array<uint8_t, s_IP_ADDRESS_LENGTH>{0, 0, 0, 0};
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
        const auto &ota = *static_cast<OtaESP32 *>(parameters);

        ArduinoOTA.begin();

        for (;;) {
            ArduinoOTA.handle();
            if (!ota.mIsOtaForced.load()) {
                auto &powerManager = PowerManager::getInstance(ota.mpLogger);
                powerManager.restartIdleTimer();
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    void OtaESP32::createAndStartWifiConnectionTimeoutTimer() {
        // reset timer if needed
        stopAndDeleteWifiConnectionTimeoutTimer();

        mWifiConnectionTimeoutTimer = xTimerCreate(
            "Wifi Con Timeout",
            ms_WIFI_CONNECTION_TIMEOUT_TIME,
            pdTRUE,
            xTaskGetCurrentTaskHandle(),
            wifiConnectionTimeoutTimerCallback
        );
        xTimerStart(mWifiConnectionTimeoutTimer, portMAX_DELAY);
    }

    void OtaESP32::stopAndDeleteWifiConnectionTimeoutTimer() {
        if (mWifiConnectionTimeoutTimer == nullptr) return;

        xTimerStop(mWifiConnectionTimeoutTimer, portMAX_DELAY);
        xTimerDelete(mWifiConnectionTimeoutTimer, portMAX_DELAY);
        mWifiConnectionTimeoutTimer = nullptr;
    }

    void OtaESP32::wifiConnectionTimeoutTimerCallback(TimerHandle_t xTimer) {
        const TaskHandle_t target = pvTimerGetTimerID(xTimer);
        xTaskNotifyGive(target);
    }
}
