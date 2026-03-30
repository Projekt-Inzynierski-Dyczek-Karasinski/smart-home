#include "debug_led.h"

#include <memory>
#include <nlohmann/json.hpp>

#include "../../config/system_config/universal_module_system_config.h"
#include "../config/user_config/critical_config.h"
#include "data_manager.h"

namespace nl = nlohmann;

namespace UniversalModuleSystem {
    DebugLED::DebugLED(const std::shared_ptr<ul::Logger> &logger) : mpLogger(logger) {
        try {
            const auto &dataManager = DataManager::getInstance();
            nl::json jsonData = dataManager.loadJson(dataManager.s_BASE_CONFIG_PATH);
            mLedPin = jsonData[ms_LED_PIN].get<uint8_t>();
        } catch (...) {
            mpLogger->error("DebugLED", "Can not load led pin from base_config.json, loading from critical_config.h");
            mLedPin = LED_PIN;
        }
        if (mLedPin != LED_PIN) {
            mpLogger->warning(
                "DebugLED",
                "Led pin defined in critical_config.h and base_config.json do not match, loading pin defined in base_config.json"
            );
        }
        pinMode(mLedPin, OUTPUT);
        digitalWrite(mLedPin, LOW);

        mpLogger->verbose("DebugLED Class", "DebugLED initialized.");
    }

    DebugLED::~DebugLED() {
        deleteBlinkTask();
        deleteResetBlinkTask();
        deleteBlinkTimeout();
        digitalWrite(mLedPin, LOW);
    }

    void DebugLED::wifiConnected() {
        deleteBlinkTask();
        deleteResetBlinkTask();
        digitalWrite(mLedPin, HIGH);
    }

    void DebugLED::wifiDisconnected() {
        deleteBlinkTask();
        deleteResetBlinkTask();
        digitalWrite(mLedPin, LOW);
    }

    void DebugLED::powerOnBlink() {
        if (mBlinkHandle == nullptr) {
            xTaskCreate(
                powerOnBlinkTask,
                "Power On Blink",
                BOOT_CHECK_TASK_SIZE,
                this,
                LOW_TASK_PRIORITY,
                nullptr
            );
        }
    }

    void DebugLED::blink(const uint32_t ledOnDuration, const uint32_t ledOffDuration) const {
        digitalWrite(mLedPin, HIGH);
        vTaskDelay(pdMS_TO_TICKS(ledOnDuration));
        digitalWrite(mLedPin, LOW);
        vTaskDelay(pdMS_TO_TICKS(ledOffDuration));
    }

    void DebugLED::deleteBlinkTask() {
        if (mBlinkHandle != nullptr) {
            deleteBlinkTimeout();

            vTaskDelete(mBlinkHandle);
            mBlinkHandle = nullptr;
            digitalWrite(mLedPin, LOW);
        }
    }

    // ====================== Pairing Blink Task ======================
    void DebugLED::pairingBlink(void *parameters) {
        auto& dl = *static_cast<DebugLED*>(parameters);

        for(;;) {
            dl.blink(CONNECTION_BLINK_DELAY, CONNECTION_BLINK_DELAY);
        }
    }
    void DebugLED::createPairingBlinkTask() {
        deleteBlinkTask();
        if (mBlinkHandle == nullptr) {
            xTaskCreate(
                pairingBlink,
                "Pairing Blink",
                PAIRING_BLINK_TASK_SIZE,
                this,
                BACKGROUND_TASK_PRIORITY,
                &mBlinkHandle
            );
        }
    }

    // ======================= Reset Blink Task =======================
    void DebugLED::resetBlink(void *parameters) {
        auto& dl = *static_cast<DebugLED*>(parameters);
        for(;;) {
            dl.blink(RESET_BLINK_DELAY, RESET_BLINK_DELAY);
        }
    }
    void DebugLED::createResetBlinkTask() {
        deleteBlinkTask();
        if (mBlinkHandle == nullptr) {
            xTaskCreate(
                resetBlink,
                "Reset Blink",
                RESET_BLINK_TASK_SIZE,
                nullptr,
                BACKGROUND_TASK_PRIORITY,
                &mBlinkHandle
            );
            startBlinkTimeout(MAX_RESET_BLINK_TIME);
        }
    }
    void DebugLED::deleteResetBlinkTask() {
        if (mBlinkHandle != nullptr) {
            deleteBlinkTimeout();

            vTaskDelete(mBlinkHandle);
            mBlinkHandle = nullptr;
            digitalWrite(mLedPin, LOW);
        }
    }

    // ===================== Blink Timeout Timer ======================
    void DebugLED::blinkTimeoutCallback(TimerHandle_t xTimer) {
        auto &dl = *static_cast<DebugLED*>(pvTimerGetTimerID(xTimer));

        if (dl.mBlinkHandle != nullptr) {
            dl.deleteBlinkTask();
        }
        dl.deleteBlinkTimeout();
    }

    void DebugLED::startBlinkTimeout(const uint32_t maxBlinkTime) {
        if (mBlinkTimeout == nullptr) {
            mBlinkTimeout = xTimerCreate(
                "Blink Timeout",
                pdMS_TO_TICKS(maxBlinkTime),
                pdFALSE,
                this,
                blinkTimeoutCallback
            );
        }

        if (mBlinkTimeout != nullptr) {
            xTimerStart(mBlinkTimeout, portMAX_DELAY);
        } else {
            mpLogger->error("DebugLED FreeRTOS", "Can't start Blink Timeout timer, because timer not exists.");
        }
    }

    void DebugLED::deleteBlinkTimeout() {
        if (mBlinkTimeout != nullptr) {
            xTimerDelete(mBlinkTimeout, portMAX_DELAY);
            mBlinkTimeout = nullptr;
        }
    }

    void DebugLED::powerOnBlinkTask(void *parameters) {
        const auto &dl = *static_cast<DebugLED *>(parameters);
        digitalWrite(dl.mLedPin, LOW);
        vTaskDelay(pdMS_TO_TICKS(100));
        digitalWrite(dl.mLedPin, HIGH);
        vTaskDelay(pdMS_TO_TICKS(100));
        digitalWrite(dl.mLedPin, LOW);

        vTaskDelete(nullptr);
        for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
