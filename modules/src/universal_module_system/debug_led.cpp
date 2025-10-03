#include "debug_led.h"

#include <memory>

#include "../config/universal_module_system_config.h"

namespace UniversalModuleSystem {
    DebugLED::DebugLED(const std::shared_ptr<ul::Logger> &logger) : mpLogger(logger) {
        pinMode(LED_PIN, OUTPUT);
        digitalWrite(LED_PIN, LOW);

        mpLogger->info("DebugLED Class", "DebugLED initialized.");
    }

    DebugLED::~DebugLED() {
        deleteBlinkTask();
        deleteResetBlinkTask();
        deleteBlinkTimeout();
        digitalWrite(LED_PIN, LOW);
    }

    void DebugLED::blink(const uint32_t ledOnDuration, const uint32_t ledOffDuration) {
        digitalWrite(LED_PIN, HIGH);
        vTaskDelay(pdMS_TO_TICKS(ledOnDuration));
        digitalWrite(LED_PIN, LOW);
        vTaskDelay(pdMS_TO_TICKS(ledOffDuration));
    }

    void DebugLED::deleteBlinkTask() {
        if (mBlinkHandle != nullptr) {
            deleteBlinkTimeout();

            vTaskDelete(mBlinkHandle);
            mBlinkHandle = nullptr;
            digitalWrite(LED_PIN, LOW);
        }
    }

    // ====================== Pairing Blink Task ======================
    void DebugLED::pairingBlink(void *parameters) {
        // TODO remove from other classes pointer to instance of class (mps[classname]) and replace it (if needed) with this:
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
            digitalWrite(LED_PIN, LOW);
        }
    }

    // ===================== Blink Timeout Timer ======================
    void DebugLED::blinkTimeoutCallback(TimerHandle_t xTimer) {
        // TODO remove from other classes pointer to instance of class (mps[classname]) and replace it (if needed) with this (timers):
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
}