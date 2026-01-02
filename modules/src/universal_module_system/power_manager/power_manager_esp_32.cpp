#include "power_manager_esp_32.h"

#include <driver/rtc_io.h>
// TODO !pr uncomment
// #include <WiFi.h>
// #include <esp_wifi.h>

#include "../../../config/universal_module_system_config.h"
#include "universal_module_system/data_manager.h"
#include "communication/communication.h"

namespace nl = nlohmann;

namespace UniversalModuleSystem {
    #ifdef AUTO_SLEEP
        RTC_DATA_ATTR uint32_t PowerManagerESP32::msSleepStart = 0;
        RTC_DATA_ATTR int64_t PowerManagerESP32::msIntendedSleepTime = 0;
    #endif

    PowerManagerESP32& PowerManagerESP32::getInstance(const std::shared_ptr<ul::Logger> &logger) {
        static PowerManagerESP32 instance(logger);
        return instance;
    }

    void PowerManagerESP32::safeRestart(const char *source) const {
        mpLogger->warning(source, "Safe Rebooting...");
        waitAndDisableCriticalFeatures();
        ESP.restart();
    }

    void PowerManagerESP32::enterSleep(const uint32_t milliSeconds, const bool enableWakeUpWithRfModule) {
        constexpr uint16_t US_TO_MILLISECONDS_FACTOR = 1000;

        // disable WiFi
        // TODO !pr uncomment
        // WiFi.disconnect(true);
        // WiFi.mode(WIFI_OFF);
        // esp_wifi_stop();

        // button wake up
        rtc_gpio_pulldown_dis(BUTTON_PIN_AS_GPIO);
        rtc_gpio_pullup_en(BUTTON_PIN_AS_GPIO);
        esp_sleep_enable_ext0_wakeup(BUTTON_PIN_AS_GPIO, LOW);

        // rf module wake up
        if (enableWakeUpWithRfModule) {
            rtc_gpio_pulldown_dis(RF_MODULE_WAKE_UP_PIN);
            rtc_gpio_pullup_en(RF_MODULE_WAKE_UP_PIN);
            // NOTE: ESP_EXT1_WAKEUP_ALL_LOW is deprecated on ESP32-S3 boards, but ESP_EXT1_WAKEUP_ANY_LOW doesn't exist on ESP32-WROOM
            // For wake up logic it doesn't matter if is all or any, because RF_MODULE_WAKE_UP_PIN_BITMASK have only one pin assigned
            #ifdef ESP32_WROOM_BOARD_TYPE
                esp_sleep_enable_ext1_wakeup(RF_MODULE_WAKE_UP_PIN_BITMASK, ESP_EXT1_WAKEUP_ALL_LOW);
            #else
                esp_sleep_enable_ext1_wakeup(RF_MODULE_WAKE_UP_PIN_BITMASK, ESP_EXT1_WAKEUP_ANY_LOW);
            #endif
        } else {
            const auto &communication = Comms::Communication::getInstance(nullptr, nullptr);
            communication.putRfModuleToSleep();
        }

        // timer wake up
        esp_sleep_enable_timer_wakeup((uint64_t)milliSeconds * US_TO_MILLISECONDS_FACTOR);

        mpLogger->infov("PowerManagerESP32", "Going to sleep for (ms): ", milliSeconds);

        waitAndDisableCriticalFeatures();

        #ifdef AUTO_SLEEP
            msIntendedSleepTime = milliSeconds;
            msSleepStart = getCurrentTime();
        #endif

        esp_deep_sleep_start();
    }

    void PowerManagerESP32::disableAutoSleep() {
        #ifdef AUTO_SLEEP
            if (mAutoSleepTimer != nullptr) {
                xTimerDelete(mAutoSleepTimer, portMAX_DELAY);
                mAutoSleepTimer = nullptr;
            }
            msIntendedSleepTime = 0;
            msSleepStart = 0;
        #endif
    }

    void PowerManagerESP32::restartIdleTimer() {
        if (mIdleTimer != nullptr) {
            xTimerStart(mIdleTimer, portMAX_DELAY);
        }
    }

    PowerManagerESP32::PowerManagerESP32(const std::shared_ptr<ul::Logger> &logger) : mpLogger(logger) {
        handleWakeUpReason();
        createIdleTimer();
    }

    PowerManagerESP32::~PowerManagerESP32() {
        if (mAutoSleepTimer != nullptr)
            xTimerDelete(mAutoSleepTimer, portMAX_DELAY);
        if (mIdleTimer != nullptr)
            xTimerDelete(mIdleTimer, portMAX_DELAY);
    }

    void PowerManagerESP32::waitAndDisableCriticalFeatures() const {
        const auto &communication = Comms::Communication::getInstance(nullptr, nullptr);
        const auto &dataManager = DataManager::getInstance();

        communication.waitAndDisableRfModule();
        dataManager.waitAndDisable();
        mpLogger->waitAndDisable();
    }

    void PowerManagerESP32::handleWakeUpReason() {
        switch (esp_sleep_get_wakeup_cause()) {
            case ESP_SLEEP_WAKEUP_EXT0:
                mpLogger->info("PowerManagerESP32 Class", "Module was wake up by Pairing Button.");
                disableAutoSleep();
                break;
            case ESP_SLEEP_WAKEUP_EXT1:
                mpLogger->info("PowerManagerESP32 Class", "Module was wake up by rf module.");
                enableAutoSleep();
                break;
            case ESP_SLEEP_WAKEUP_TIMER:
                mpLogger->info("PowerManagerESP32 Class", "Module was wake up by timer.");
                disableAutoSleep();
                break;
            default:
                mpLogger->info("PowerManagerESP32 Class", "Module had power loss.");
                disableAutoSleep();
                break;
        }
    }

    void PowerManagerESP32::enableAutoSleep() {
        #ifdef AUTO_SLEEP
            // constexpr uint16_t US_TO_MILLISECONDS_FACTOR = 1000;
            const uint32_t sleepTime = getCurrentTime() - msSleepStart;
            msIntendedSleepTime -= (sleepTime + (AUTO_SLEEP_WAIT_TIME));
            if (msIntendedSleepTime > 0) {
                mAutoSleepTimer = xTimerCreate(
                    "Auto Sleep Timer",
                    pdMS_TO_TICKS(AUTO_SLEEP_WAIT_TIME),
                    pdFALSE,
                    this,
                    timerTriggeredSleep
                );
                xTimerStart(mAutoSleepTimer, portMAX_DELAY);
            }
        #endif
    }

    void PowerManagerESP32::goToAutoSleep(TimerHandle_t xTimer) {
        #ifdef AUTO_SLEEP
            auto &pm = *static_cast<PowerManagerESP32 *>(pvTimerGetTimerID(xTimer));
            pm.enterSleep(msIntendedSleepTime , true);
        #endif
    }

    uint32_t PowerManagerESP32::getCurrentTime() const {
        struct timeval timeStruct{};
        gettimeofday(&timeStruct, nullptr);
        return timeStruct.tv_sec * 1000;
    }

    void PowerManagerESP32::createIdleTimer() {
#ifndef CENTRAL_UNIT
        const auto &dm = DataManager::getInstance();
        nl::json jsonData = dm.loadJson(dm.s_BASE_CONFIG_PATH);
        nl::json &idleTimerData = jsonData[ms_IDLE_TIMER_DATA];
        const uint32_t sleepTime = idleTimerData[ms_IDLE_TIMER_SLEEP_TIME].get<uint32_t>();
        const uint32_t timeout = idleTimerData[ms_IDLE_TIMER_TIMEOUT].get<uint32_t>();

        if (sleepTime != 0 && timeout != 0) {
            mIdleSleepTime.store(sleepTime);
            mIdleTimer = xTimerCreate(
                "Idle Timer",
                pdMS_TO_TICKS(timeout),
                pdFALSE,
                this,
                idleAutosleep
            );
            xTimerStart(mIdleTimer, portMAX_DELAY);
        }
#endif
    }

    void PowerManagerESP32::idleAutosleep(TimerHandle_t xTimer) {
        auto &pm = *static_cast<PowerManagerESP32 *>(pvTimerGetTimerID(xTimer));
        if (const uint32_t sleepTime = pm.mIdleSleepTime.load(); sleepTime != 0)
            pm.enterSleep(sleepTime, true);
    }
}
