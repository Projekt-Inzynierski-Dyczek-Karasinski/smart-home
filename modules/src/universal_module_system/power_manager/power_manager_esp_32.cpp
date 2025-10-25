#include "power_manager_esp_32.h"

#include <driver/rtc_io.h>

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
        esp_sleep_enable_timer_wakeup(milliSeconds * US_TO_MILLISECONDS_FACTOR);

        mpLogger->infov("PowerManagerESP32", "Going to sleep for (ms): ", milliSeconds);

        waitAndDisableCriticalFeatures();

        #ifdef AUTO_SLEEP
            msIntendedSleepTime = milliSeconds;
            msSleepStart = getCurrentTime();
        #endif

        esp_deep_sleep_start();
    }

    uint16_t PowerManagerESP32::getBatteryRead() const {
        xSemaphoreTake(mReadCompleteSemaphore, portMAX_DELAY);
        xSemaphoreGive(mReadCompleteSemaphore);
        return mBatteryRead.load();
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

    PowerManagerESP32::PowerManagerESP32(const std::shared_ptr<ul::Logger> &logger) : mpLogger(logger) {
        handleWakeUpReason();
        mReadCompleteSemaphore = xSemaphoreCreateBinary();
        readBattery();
    }

    PowerManagerESP32::~PowerManagerESP32() {
        xTimerDelete(mAutoSleepTimer, portMAX_DELAY);
        vSemaphoreDelete(mReadCompleteSemaphore);
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
                mpLogger->verbose("PowerManagerESP32 Class", "Module was wake up by Pairing Button.");
                disableAutoSleep();
                break;
            case ESP_SLEEP_WAKEUP_EXT1:
                mpLogger->verbose("PowerManagerESP32 Class", "Module was wake up by rf module.");
                enableAutoSleep();
                break;
            case ESP_SLEEP_WAKEUP_TIMER:
                mpLogger->verbose("PowerManagerESP32 Class", "Module was wake up by timer.");
                disableAutoSleep();
                break;
            default:
                mpLogger->verbose("PowerManagerESP32 Class", "Module had power loss.");
                disableAutoSleep();
                break;
        }
    }

    uint16_t PowerManagerESP32::calculateVoltage(const uint16_t rawAnalogRead)  {
        constexpr uint16_t MAX_ANALOG_READ = 4095;
        constexpr uint16_t MAX_ANALOG_READ_MILLIVOLTAGE = 3300;

        const auto &dataManager = ums::DataManager::getInstance();
        const nl::json jsonData = dataManager.loadJson(POWER_DATA_PATH);
        if (jsonData.empty()) {
            mpLogger->error("PowerManagerESP32 Task", "Failed to load power data from SPISFFS.");
            return rawAnalogRead;
        }

        const PowerData powerData(jsonData);
        // uint64_t to not exceed overflow in calculations
        uint64_t result = rawAnalogRead;
        result *= MAX_ANALOG_READ_MILLIVOLTAGE;
        result *= (powerData.vccResistor + powerData.gndResistor);
        result /= powerData.gndResistor;
        result /= MAX_ANALOG_READ;

        // TODO !pr remove
        // size_t size2 = snprintf(nullptr, 0, "%i,", rawAnalogRead);
        // char valueToSave2[size2];
        // sprintf(valueToSave2, "%i,", rawAnalogRead);
        //
        // dataManager.tmpSave("/root/raw", valueToSave2);

        // TODO !pr remove
        // size_t size = snprintf(nullptr, 0, "%i,", (uint16_t)result);
        // char valueToSave[size];
        // sprintf(valueToSave, "%i,", (uint16_t)result);
        //
        // dataManager.tmpSave("/root/br", valueToSave);

        return (uint16_t)result;
    }

    void PowerManagerESP32::batteryReadTask(void* parameters) {
        auto& pm = *static_cast<PowerManagerESP32*>(parameters);
        constexpr uint16_t timeBetweenReads = 50;
        constexpr uint8_t numberOfReads = 5;
        uint16_t batteryReadSum = 0;

        for (uint8_t i = 0; i < numberOfReads; i++) {
            vTaskDelay(pdMS_TO_TICKS(timeBetweenReads));
            batteryReadSum += analogRead(BATTERY_PIN);
        }

        pm.mBatteryRead.store(pm.calculateVoltage(batteryReadSum /= numberOfReads));

        pm.mpLogger->verbosev("PowerManagerESP32 Task", "Battery charge level is (mV): ", pm.mBatteryRead.load());
        xSemaphoreGive(pm.mReadCompleteSemaphore);
        vTaskDelete(nullptr);
    }

    // TODO !pr make battery read as sensor in transducers
    void PowerManagerESP32::readBattery() {
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
                    goToAutoSleep
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
        // TODO consider making this more accurate
        return timeStruct.tv_sec * 1000;
    }

    PowerManagerESP32::PowerData::PowerData(const nlohmann::json &json) {
        nlohmann::json data = json[ms_DATA_PATH];
        vccResistor = data[ms_VCC_RESISTOR_PATH].get<uint32_t>();
        gndResistor = data[ms_GND_RESISTOR_PATH].get<uint32_t>();
    }

}