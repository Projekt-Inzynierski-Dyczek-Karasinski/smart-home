#include "power_manager_esp_32.h"

#include <driver/rtc_io.h>

#include "universal_module_system/ota/ota.h"

#define RF_MODULE_WAKE_UP_PIN_BITMASK(gpio_num) 1ULL << gpio_num

#include "../../../config/system_config/universal_module_system_config.h"
#include "../config/user_config/critical_config.h"

#include "universal_module_system/data_manager.h"
#include "communication/communication.h"
#include "communication/api/command_handler.h"

#ifdef HC12_MODULE
#include "communication/hc12.h"
#endif

namespace nl = nlohmann;
namespace API = Comms::API;

namespace UniversalModuleSystem {
    PowerManagerESP32& PowerManagerESP32::getInstance(const std::shared_ptr<ul::Logger> &logger) {
        static PowerManagerESP32 instance(logger);
        return instance;
    }

    void PowerManagerESP32::safeRestart(const char *source) const {
        mpLogger->warning(source, "Safe Rebooting...");
        waitAndDisableCriticalFeatures();
        esp_restart();
    }

    void PowerManagerESP32::enterSleep(const uint32_t milliSeconds, const bool enableWakeUpWithRfModule) {
        #ifdef HC12_MODULE
            const auto &dataManager = DataManager::getInstance();
            using HC12 = Comms::HC12;
            nl::json jsonData = dataManager.loadJson(dataManager.s_BASE_CONFIG_PATH);
            const gpio_num_t hc12WakeUpPin = jsonData[HC12::s_HC12_DATA][HC12::s_RX_PIN].get<gpio_num_t>();
        #else
        #error "Not implemented"
        #endif
        
        constexpr uint16_t US_TO_MILLISECONDS_FACTOR = 1000;

        // disable WiFi and ota
        auto &ota = Ota::getInstance();
        ota.endOta();

        // button wake up
        rtc_gpio_pulldown_dis(static_cast<gpio_num_t>(BUTTON_PIN));
        rtc_gpio_pullup_en(static_cast<gpio_num_t>(BUTTON_PIN));
        esp_sleep_enable_ext0_wakeup(static_cast<gpio_num_t>(BUTTON_PIN), LOW);

        // rf module wake up
        if (enableWakeUpWithRfModule) {
            rtc_gpio_pulldown_dis(hc12WakeUpPin);
            rtc_gpio_pullup_en(hc12WakeUpPin);
            // NOTE: ESP_EXT1_WAKEUP_ALL_LOW is deprecated on ESP32-S3 boards, but ESP_EXT1_WAKEUP_ANY_LOW doesn't exist on ESP32-WROOM
            // For wake up logic it doesn't matter if is all or any, because RF_MODULE_WAKE_UP_PIN_BITMASK have only one pin assigned
            #ifdef ESP32_WROOM_BOARD_TYPE
                esp_sleep_enable_ext1_wakeup(RF_MODULE_WAKE_UP_PIN_BITMASK(hc12WakeUpPin), ESP_EXT1_WAKEUP_ALL_LOW);
            #else
                esp_sleep_enable_ext1_wakeup(RF_MODULE_WAKE_UP_PIN_BITMASK(hc12WakeUpPin), ESP_EXT1_WAKEUP_ANY_LOW);
            #endif
        } else {
            const auto &communication = Comms::Communication::getInstance(nullptr, nullptr);
            communication.putRfModuleToSleep();
        }

        // timer wake up
        esp_sleep_enable_timer_wakeup((uint64_t)milliSeconds * US_TO_MILLISECONDS_FACTOR);

        mpLogger->infov("PowerManagerESP32", "Going to sleep for (ms): ", milliSeconds);

        waitAndDisableCriticalFeatures();

        esp_deep_sleep_start();
    }

    void PowerManagerESP32::restartIdleTimer() {
        if (mIdleTimer != nullptr) {
            xTimerStart(mIdleTimer, portMAX_DELAY);
        }
    }

    bool PowerManagerESP32::wasModuleRestarted() const {
        switch (esp_sleep_get_wakeup_cause()) {
            // add here more wake up reasons if needed
            case ESP_SLEEP_WAKEUP_EXT1:
            case ESP_SLEEP_WAKEUP_TIMER:
            case ESP_SLEEP_WAKEUP_EXT0:
                return false;
            default:
                return true;
        }
    }

    PowerManagerESP32::PowerManagerESP32(const std::shared_ptr<ul::Logger> &logger) : mpLogger(logger) {
        if (logger == nullptr) {
            mpLogger = std::make_shared<ul::Logger>();
            mpLogger->error("PowerManagerESP32", "PowerManagerESP32's constructor didn't get pointer to logger instance.");
        }
        handleWakeUpReason();
        createIdleTimer();
    }

    PowerManagerESP32::~PowerManagerESP32() {
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

    void PowerManagerESP32::handleWakeUpReason() const {
        switch (esp_sleep_get_wakeup_cause()) {
            case ESP_SLEEP_WAKEUP_EXT1:
                mpLogger->info("PowerManagerESP32 Class", "Module was wake up by rf module.");
                break;

            case ESP_SLEEP_WAKEUP_TIMER:
                mpLogger->info("PowerManagerESP32 Class", "Module was wake up by timer.");
                break;

// macro disabling wake up rf notifications that are annoying during software development
#ifdef DISABLE_WAKE_UP_RF_NOTIFICATION
#warning "Wake up rf notifications are disabled"
            case ESP_SLEEP_WAKEUP_EXT0:
                mpLogger->info("PowerManagerESP32 Class", "Module was wake up by Pairing Button.");
                break;

            default:
                mpLogger->info("PowerManagerESP32 Class", "Module had power loss.");
                break;

#else
            case ESP_SLEEP_WAKEUP_EXT0:
                try {
                    API::CommandHandler commandHandler(API::commandTypes::NOTIFY);
                    API::APIParameter notify(static_cast<uint8_t>(API::notifyTypes::MANUAL_WAKE_UP));
                    commandHandler.addParameter(notify);

                    uint8_t message[MESSAGE_SIZE] = {};
                    commandHandler.generateMessage(message);
                    const auto &communication = Comms::Communication::getInstance();
                    communication.sendMessage(message);
                } catch (std::exception &e) {
                    mpLogger->error("PowerManagerESP32 handleWakeUpReason", "Failed to create notification in case ESP_SLEEP_WAKEUP_EXT0.");
                    mpLogger->error("PowerManagerESP32 handleWakeUpReason", e.what());
                }
                mpLogger->info("PowerManagerESP32 Class", "Module was wake up by Pairing Button.");
                break;

            default:
                try {
                    API::CommandHandler commandHandler(API::commandTypes::NOTIFY);
                    API::APIParameter notify(static_cast<uint8_t>(API::notifyTypes::POWER_LOSS));
                    commandHandler.addParameter(notify);

                    uint8_t message[MESSAGE_SIZE] = {};
                    commandHandler.generateMessage(message);
                    const auto &communication = Comms::Communication::getInstance();
                    communication.sendMessage(message);
                } catch (std::exception &e) {
                    mpLogger->error("PowerManagerESP32 handleWakeUpReason", "Failed to create notification in default case.");
                    mpLogger->error("PowerManagerESP32 handleWakeUpReason", e.what());
                }
                mpLogger->info("PowerManagerESP32 Class", "Module had power loss.");
                break;
#endif
        }
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
