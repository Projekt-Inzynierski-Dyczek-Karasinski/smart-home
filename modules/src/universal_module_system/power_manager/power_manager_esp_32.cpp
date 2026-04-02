#include "power_manager_esp_32.h"

#include <driver/rtc_io.h>

#include "universal_module_system/ota/ota.h"

#include "../../../config/system_config/universal_module_system_config.h"
#include "../config/user_config/critical_config.h"

#include "universal_module_system/data_manager.h"
#include "universal_module_system/pairing_button.h"
#include "universal_module_system/transducers/sensors/sensors_manager.h"
#include "communication/communication.h"
#include "communication/api/command_handler.h"

#ifdef HC12_MODULE
#include "communication/hc12.h"
#endif

namespace nl = nlohmann;
namespace API = Comms::API;

namespace UniversalModuleSystem {
    RTC_DATA_ATTR bool PowerManagerESP32::isSensorUsingExt0 = false;

    PowerManagerESP32 &PowerManagerESP32::getInstance(
        const std::shared_ptr<ul::Logger> &logger,
        const std::shared_ptr<DebugLED> &debugLED
    ) {
        static PowerManagerESP32 instance(logger, debugLED);
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

        const gpio_num_t buttonPin = static_cast<gpio_num_t>(PairingButton::getButtonPin(mpLogger));
        // EXT0 is reserved for sensor wake up, but ESP32 WROOM does not support ESP_EXT1_WAKEUP_ANY_LOW, so
        // waking up ESP by sensor is only available on ESP32-S3
        #ifdef ESP32_WROOM_BOARD_TYPE
        // button wake up

        rtc_gpio_pulldown_dis(buttonPin);
        rtc_gpio_pullup_en(buttonPin);
        esp_sleep_enable_ext0_wakeup(buttonPin, LOW);

        // rf module wake up
        // TODO add changing FU mode for power saving
        if (enableWakeUpWithRfModule) {
            rtc_gpio_pulldown_dis(hc12WakeUpPin);
            rtc_gpio_pullup_en(hc12WakeUpPin);
            // NOTE: ESP_EXT1_WAKEUP_ALL_LOW is deprecated on ESP32-S3 boards, but ESP_EXT1_WAKEUP_ANY_LOW doesn't exist on ESP32-WROOM
            // For wake up logic it doesn't matter if is all or any, because pin bitmask have only one pin assigned
            esp_sleep_enable_ext1_wakeup((1ULL << hc12WakeUpPin), ESP_EXT1_WAKEUP_ALL_LOW);
        } else {
            const auto &communication = Comms::Communication::getInstance(nullptr, nullptr);
            communication.putRfModuleToSleep();
        }
        #else
        // handle sensor onSleep
        auto &sensorManager = Transducers::SensorsManager::getInstance();
        sensorManager.onSleep();

        const bool isButtonWakeUpAssigned = addWakeUpOnEXT0(buttonPin, LOW);
        isSensorUsingExt0 = !isButtonWakeUpAssigned;

        // rf module wake up
        const auto &communication = Comms::Communication::getInstance(nullptr, nullptr);
        rtc_gpio_pulldown_dis(buttonPin);
        rtc_gpio_pullup_en(buttonPin);
        if (enableWakeUpWithRfModule) {
            // TODO !pr add check for "hc12": {(...), "isPowerSavingEnabled": true}
            communication.putRFModuleInPowerSavingMode(); // TODO change name
            rtc_gpio_pulldown_dis(hc12WakeUpPin);
            rtc_gpio_pullup_en(hc12WakeUpPin);
            esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
            // esp_ext1
            const uint64_t bitMask =
                    isButtonWakeUpAssigned ? (1ULL << hc12WakeUpPin) : (1ULL << hc12WakeUpPin) | (1ULL << buttonPin);
            esp_sleep_enable_ext1_wakeup(bitMask, ESP_EXT1_WAKEUP_ANY_LOW);
        } else {
            if (!isButtonWakeUpAssigned)
                esp_sleep_enable_ext1_wakeup((1ULL << buttonPin), ESP_EXT1_WAKEUP_ANY_LOW);
            communication.putRfModuleToSleep();
        }
        #endif

        // timer wake up
        esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(milliSeconds) * US_TO_MILLISECONDS_FACTOR);

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

    bool PowerManagerESP32::addWakeUpOnEXT0(const gpio_num_t pin, const bool level) const {
        if (xSemaphoreTake(mEspExt0Available, 0) == pdFALSE) {
            mpLogger->verbose("PowerManagerESP32", "Wake up on EXT0 is already assigned.");
            return false;
        }

        rtc_gpio_pulldown_dis(pin);
        rtc_gpio_pullup_en(pin);
        esp_sleep_enable_ext0_wakeup(pin, level);
        return true;
    }

    void PowerManagerESP32::setupComplete() {
        // TODO !pr add check for "idleAutoSleep": {(...), "enterSleepAfterWakeUpByTimer": true}
        if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER && false) {
            xTaskCreate(
                enterSleepAfterBootTask,
                "Enter Sleep Task",
                ENTER_SLEEP_TASK_SIZE,
                this,
                BACKGROUND_TASK_PRIORITY,
                nullptr
            );
        }
    }


    PowerManagerESP32::PowerManagerESP32(
        const std::shared_ptr<ul::Logger> &logger,
        const std::shared_ptr<DebugLED> &debugLED
    ) : mpLogger(logger),
        mpDebugLED(debugLED) {
        if (logger == nullptr) {
            mpLogger = std::make_shared<ul::Logger>();
            mpLogger->error(
                "PowerManagerESP32",
                "PowerManagerESP32's constructor didn't get pointer to logger instance."
            );
        }
        if (debugLED == nullptr) {
            mpLogger->error(
                "PowerManagerESP32",
                "PowerManagerESP32's constructor didn't get pointer to debugLED instance."
            );
        }

        mEspExt0Available = xSemaphoreCreateBinary();
        xSemaphoreGive(mEspExt0Available);

        handleWakeUpReason();
        createIdleTimer();
    }

    PowerManagerESP32::~PowerManagerESP32() {
        if (mIdleTimer != nullptr) {
            xTimerDelete(mIdleTimer, portMAX_DELAY);
            mIdleTimer = nullptr;
        }
    }

    void PowerManagerESP32::waitAndDisableCriticalFeatures() const {
        const auto &communication = Comms::Communication::getInstance(nullptr, nullptr);
        const auto &dataManager = DataManager::getInstance();

        vTaskDelay(pdMS_TO_TICKS(100));

        communication.waitAndDisableRfModule();
        dataManager.waitAndDisable();
        mpLogger->waitAndDisable();
    }

    void PowerManagerESP32::handleWakeUpReason() {
        switch (esp_sleep_get_wakeup_cause()) {
            case ESP_SLEEP_WAKEUP_TIMER:
                mpLogger->info("PowerManagerESP32 Class", "Module was wake up by timer.");
                break;

                // macro disabling wake up rf notifications that are annoying during software development
                #ifdef DISABLE_WAKE_UP_RF_NOTIFICATION
                #warning "Wake up rf notifications are disabled"
            case ESP_SLEEP_WAKEUP_EXT1: {
                const uint64_t mask = esp_sleep_get_ext1_wakeup_status();
                for (uint8_t gpio = 0; gpio < 64; gpio++) {
                    if (mask & (1ULL << gpio)) {
                        mpLogger->infov("PowerManagerESP32 Class", "Module was wake up by by GPIO %d\n", gpio);
                    }
                }
                break;
            }
            // TODO add here check notif from window sensor (both here and in #else)
            case ESP_SLEEP_WAKEUP_EXT0:
                // CHECKME
                if (isSensorUsingExt0) {
                    #ifndef CENTRAL_UNIT
                    try {
                        API::CommandHandler commandHandler(API::commandTypes::NOTIFY);
                        API::APIParameter notify(static_cast<uint8_t>(API::notifyTypes::SENSOR_ALERT));
                        commandHandler.addParameter(notify);

                        uint8_t message[MESSAGE_SIZE] = {};
                        commandHandler.generateMessage(message);

                        const auto &communication = Comms::Communication::getInstance();
                        communication.sendMessage(message);
                    } catch (std::exception &e) {
                        mpLogger->error(
                            "PowerManagerESP32 handleWakeUpReason",
                            "Failed to create notification in case ESP_SLEEP_WAKEUP_EXT0."
                        );
                        mpLogger->error("PowerManagerESP32 handleWakeUpReason", e.what());
                    }
                    #endif
                }
                mpLogger->info("PowerManagerESP32 Class", "Module was wake up by ESP_SLEEP_WAKEUP_EXT0.");
                break;

            default:
                mpDebugLED->powerOnBlink();
                mpLogger->info("PowerManagerESP32 Class", "Module had power loss.");
                break;

                #else
            case ESP_SLEEP_WAKEUP_EXT0:
                // TODO change logic for #else
                try {
                    API::CommandHandler commandHandler(API::commandTypes::NOTIFY);



                #ifdef ESP32_WROOM_BOARD_TYPE
                API::APIParameter notify(static_cast<uint8_t>(API::notifyTypes::MANUAL_WAKE_UP));
                #else
                API::APIParameter notify(static_cast<uint8_t>(API::notifyTypes::SENSOR_ALERT));
                #endif
                commandHandler.addParameter(notify);

                uint8_t message[MESSAGE_SIZE] = {};
                commandHandler.generateMessage(message);
                const auto &communication = Comms::Communication::getInstance();
                communication.sendMessage(message);
                } catch (std::exception & e) {
                    mpLogger->error(
                        "PowerManagerESP32 handleWakeUpReason",
                        "Failed to create notification in case ESP_SLEEP_WAKEUP_EXT0."
                    );
                    mpLogger->error("PowerManagerESP32 handleWakeUpReason", e.what());
                }
                mpLogger->info("PowerManagerESP32 Class", "Module was wake up by EXT0.");
                break;

            case ESP_SLEEP_WAKEUP_EXT1: {
                const uint64_t mask = esp_sleep_get_ext1_wakeup_status();
                for (uint8_t gpio = 0; gpio < 64; gpio++) {
                    if (!(mask & (1ULL << gpio))) continue;

                    mpLogger->infov("PowerManagerESP32 Class", "Module was wake up by by GPIO: ", gpio);
                    if (PairingButton::getButtonPin() != gpio) continue;

                    try {
                        API::CommandHandler commandHandler(API::commandTypes::NOTIFY);
                        API::APIParameter notify(static_cast<uint8_t>(API::notifyTypes::MANUAL_WAKE_UP));
                        uint8_t message[MESSAGE_SIZE] = {};
                        commandHandler.generateMessage(message);
                        const auto &communication = Comms::Communication::getInstance();
                        communication.sendMessage(message);
                    } catch (std::exception &e) {
                        mpLogger->error(
                            "PowerManagerESP32 handleWakeUpReason",
                            "Failed to create notification in case ESP_SLEEP_WAKEUP_EXT1."
                        );
                        mpLogger->error("PowerManagerESP32 handleWakeUpReason", e.what());
                    }
                    break;
                }
                break;
            }

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
                    mpLogger->error(
                        "PowerManagerESP32 handleWakeUpReason",
                        "Failed to create notification in default case."
                    );
                    mpLogger->error("PowerManagerESP32 handleWakeUpReason", e.what());
                }
                mpDebugLED->powerOnBlink();
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

    void PowerManagerESP32::enterSleepAfterBootTask(void *parameters) {
        auto &pm = *static_cast<PowerManagerESP32 *>(parameters);
        vTaskDelay(pdMS_TO_TICKS(1000));

        const auto &dm = DataManager::getInstance();
        nl::json jsonData = dm.loadJson(dm.s_BASE_CONFIG_PATH);
        nl::json &idleTimerData = jsonData[ms_IDLE_TIMER_DATA];
        const uint32_t sleepTime = idleTimerData[ms_IDLE_TIMER_SLEEP_TIME].get<uint32_t>();

        pm.enterSleep(sleepTime, true);
        for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
