#include "power_manager.h"

#include <driver/rtc_io.h>

#include "data_manager.h"
#include "../config/universal_module_system_config.h"

#include "communication/communication.h"

namespace UniversalModuleSystem {
    #ifdef AUTO_SLEEP
        RTC_DATA_ATTR uint32_t PowerManager::msSleepStart = 0;
        RTC_DATA_ATTR int64_t PowerManager::msIntendedSleepTime = 0;
    #endif

    PowerManager& PowerManager::getInstance(const std::shared_ptr<ul::Logger> &logger) {
        static PowerManager instance(logger);
        return instance;
    }

    void PowerManager::safeRestart(const char *source) const {
        mpLogger->warning(source, "Safe Rebooting...");
        waitAndDisableCriticalFeatures();
        ESP.restart();
    }

    void PowerManager::enterSleep(const uint32_t milliSeconds, const bool enableWakeUpWithRfModule) {
        constexpr uint16_t US_TO_MILLISECONDS_FACTOR = 1000;

        // button wake up
        rtc_gpio_pulldown_dis(BUTTON_PIN_AS_GPIO);
        rtc_gpio_pullup_en(BUTTON_PIN_AS_GPIO);
        esp_sleep_enable_ext0_wakeup(BUTTON_PIN_AS_GPIO, LOW);

        // rf module wake up
        if (enableWakeUpWithRfModule) {
            rtc_gpio_pulldown_dis(RF_MODULE_WAKE_UP_PIN);
            rtc_gpio_pullup_en(RF_MODULE_WAKE_UP_PIN);
            // NOTE: ESP_EXT1_WAKEUP_ALL_LOW is deprecated on ESP32-S3 boards, but ESP_EXT1_WAKEUP_ANY_LOW doesn't exist on ESP32-WROOM
            // For wake up logic it doesn't matter  if is all or any, because RF_MODULE_WAKE_UP_PIN_BITMASK have only one pin assigned
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
        esp_sleep_enable_timer_wakeup(milliSeconds * US_TO_MILLISECONDS_FACTOR);

        mpLogger->infov("PowerManager", "Going to sleep for (ms): ", milliSeconds);

        waitAndDisableCriticalFeatures();

        #ifdef AUTO_SLEEP
            msIntendedSleepTime = milliSeconds;
            msSleepStart = getCurrentTime();
        #endif

        esp_deep_sleep_start();
    }

    uint16_t PowerManager::getBatteryRead() const {
        xSemaphoreTake(mReadCompleteSemaphore, portMAX_DELAY);
        xSemaphoreGive(mReadCompleteSemaphore);
        return mBatteryRead.load();
    }

    void PowerManager::disableAutoSleep() {
        #ifdef AUTO_SLEEP
            if (mAutoSleepTimer != nullptr) {
                xTimerDelete(mAutoSleepTimer, portMAX_DELAY);
                mAutoSleepTimer = nullptr;
            }
            msIntendedSleepTime = 0;
            msSleepStart = 0;
        #endif
    }

    PowerManager::PowerManager(const std::shared_ptr<ul::Logger> &logger) : mpLogger(logger) {
        handleWakeUpReason();
        mReadCompleteSemaphore = xSemaphoreCreateBinary();
        readBattery();
    }

    PowerManager::~PowerManager() {
        xTimerDelete(mAutoSleepTimer, portMAX_DELAY);
        vSemaphoreDelete(mReadCompleteSemaphore);
    }

    void PowerManager::waitAndDisableCriticalFeatures() const {
        const auto &communication = Comms::Communication::getInstance(nullptr, nullptr);
        const auto &dataManager = DataManager::getInstance();

        communication.waitAndDisableRfModule();
        dataManager.waitAndDisable();
        mpLogger->waitAndDisable();
    }

    void PowerManager::handleWakeUpReason() {
        switch (esp_sleep_get_wakeup_cause()) {
            case ESP_SLEEP_WAKEUP_EXT0:
                mpLogger->info("PowerManager Class", "Module was wake up by Pairing Button.");
                disableAutoSleep();
                break;
            case ESP_SLEEP_WAKEUP_EXT1:
                mpLogger->info("PowerManager Class", "Module was wake up by rf module.");
                enableAutoSleep();
                break;
            case ESP_SLEEP_WAKEUP_TIMER:
                mpLogger->info("PowerManager Class", "Module was wake up by timer.");
                disableAutoSleep();
                break;
            default:
                mpLogger->info("PowerManager Class", "Module had power loss.");
                disableAutoSleep();
                break;
        }
    }

    void PowerManager::batteryReadTask(void* parameters) {
        auto& pm = *static_cast<PowerManager*>(parameters);
        constexpr uint16_t timeBetweenReads = 50;
        constexpr uint8_t numberOfReads = 5;
        uint16_t batteryReadSum = 0;

        for (uint8_t i = 0; i < numberOfReads; i++) {
            vTaskDelay(pdMS_TO_TICKS(timeBetweenReads));
            batteryReadSum += analogRead(BATTERY_PIN);
        }

        pm.mBatteryRead.store(batteryReadSum / numberOfReads);
        // TODO add convertion from raw analog read to volts or %
        pm.mpLogger->infov("PowerManager Task", "Battery charge level is: ", pm.mBatteryRead.load());

        xSemaphoreGive(pm.mReadCompleteSemaphore);
        vTaskDelete(nullptr);
    }

    void PowerManager::readBattery() {
        // make sure that semaphore indicates that battery read is not completed
        xSemaphoreTake(mReadCompleteSemaphore, 0);

        xTaskCreate(
            batteryReadTask,
            "Battery Read Task",
            BATTERY_READ_TASK_SIZE,
            this,
            LOW_TASK_PRIORITY,
            nullptr
        );
    }

    void PowerManager::enableAutoSleep() {
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
                    goToAutoSleep
                );
                xTimerStart(mAutoSleepTimer, portMAX_DELAY);
            }
        #endif
    }

    void PowerManager::goToAutoSleep(TimerHandle_t xTimer) {
        #ifdef AUTO_SLEEP
            auto &pm = *static_cast<PowerManager *>(pvTimerGetTimerID(xTimer));
            pm.enterSleep(msIntendedSleepTime , true);
        #endif
    }

    uint32_t PowerManager::getCurrentTime() const {
        struct timeval timeStruct{};
        gettimeofday(&timeStruct, nullptr);
        // TODO consider making this more accurate
        return timeStruct.tv_sec * 1000;
    }
}