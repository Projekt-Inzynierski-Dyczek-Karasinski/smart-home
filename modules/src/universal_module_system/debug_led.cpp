#include "debug_led.h"

#include <memory>

#include "smart_home_config.h"
#include "utils/logger.h"

#define CONNECTION_BLINK_DELAY 500
#define RESET_BLINK_DELAY 100

// TODO assign final value
#define MAX_RESET_BLINK_TIME 3000


DebugLED* DebugLED::mspInstance = nullptr;

TaskHandle_t DebugLED::msPairingBlinkHandle = nullptr;
TaskHandle_t DebugLED::msResetBlinkHandle = nullptr;
TimerHandle_t DebugLED::msBlinkTimeout = nullptr;

DebugLED* DebugLED::getInstance(const std::shared_ptr<ul::Logger> &logger) {
    if (mspInstance == nullptr) {
        mspInstance = new DebugLED(logger);
    }
    return mspInstance;
}

DebugLED::DebugLED(const std::shared_ptr<ul::Logger> &logger) {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    mpLogger = logger;
    mpLogger->info("DebugLED Class", "DebugLED initialized.");
}

DebugLED::~DebugLED() {
    deletePairingBlinkTask();
    deleteResetBlinkTask();
    deleteBlinkTimeout();
    digitalWrite(LED_PIN, LOW);
}


// =========================== Getters ============================

TaskHandle_t DebugLED::getPairingBlinkHandle() {
    return msPairingBlinkHandle;
}

TaskHandle_t DebugLED::getResetBlinkHandle() {
    return msResetBlinkHandle;
}
// ================================================================

void DebugLED::blink(const uint32_t ledOnDuration, const uint32_t ledOffDuration) {
    digitalWrite(LED_PIN, HIGH);
    vTaskDelay(pdMS_TO_TICKS(ledOnDuration));
    digitalWrite(LED_PIN, LOW);
    vTaskDelay(pdMS_TO_TICKS(ledOffDuration));
}

// ====================== Pairing Blink Task ======================

void DebugLED::pairingBlink() {
    for(;;) {
        blink(CONNECTION_BLINK_DELAY, CONNECTION_BLINK_DELAY);
    }
}
void DebugLED::createPairingBlinkTaskHandle(void *parameters) {
    const DebugLED* instance = static_cast<DebugLED*>(parameters);
    instance->pairingBlink();
}
void DebugLED::createPairingBlinkTask() {
    if (msPairingBlinkHandle == nullptr) {
        deleteResetBlinkTask();

        xTaskCreate(
            createPairingBlinkTaskHandle,
            "Pairing Blink",
            PAIRING_BLINK_TASK_SIZE,
            nullptr,
            BACKGROUND_TASK_PRIORITY,
            &msPairingBlinkHandle
        );
    } else {
        mspInstance->mpLogger->warning("DebugLED FreeRTOS", "Can't create Connection Blink task, because task already exists.");
    }
}
void DebugLED::deletePairingBlinkTask() {
    if (msPairingBlinkHandle != nullptr) {
        deleteBlinkTimeout();
        
        vTaskDelete(msPairingBlinkHandle);
        msPairingBlinkHandle = nullptr;
        digitalWrite(LED_PIN, LOW);
    }
}
// ================================================================

// ======================= Reset Blink Task =======================

void DebugLED::resetBlink() {
    for(;;) {
        blink(RESET_BLINK_DELAY, RESET_BLINK_DELAY);
    }
}
void DebugLED::createResetBlinkTaskHandle(void *parameters) {
    const DebugLED* instance = static_cast<DebugLED*>(parameters);
    instance->resetBlink();
}
void DebugLED::createResetBlinkTask() {
    if (msResetBlinkHandle == nullptr) {
        deletePairingBlinkTask();
        
        xTaskCreate(
            createResetBlinkTaskHandle,
            "Reset Blink",
            RESET_BLINK_TASK_SIZE,
            nullptr,
            1,
            &msResetBlinkHandle
        );
        startBlinkTimeout(MAX_RESET_BLINK_TIME);
    } else {
        mspInstance->mpLogger->warning("DebugLED FreeRTOS", "Can't create Reset Blink task, because task already exists.");
    }
}
void DebugLED::deleteResetBlinkTask() {
    if (msResetBlinkHandle != nullptr) {
        deleteBlinkTimeout();
        
        vTaskDelete(msResetBlinkHandle);
        msResetBlinkHandle = nullptr;
        digitalWrite(LED_PIN, LOW);
    }
}
// ================================================================

// ===================== Blink Timeout Timer ======================

void DebugLED::blinkTimeoutCallback() {
    if (msBlinkTimeout != nullptr) {
        if (msPairingBlinkHandle != nullptr) {
            deletePairingBlinkTask();
        }
        
        if (msResetBlinkHandle != nullptr) {
            deleteResetBlinkTask();
        }

        deleteBlinkTimeout();
    } else {
        mspInstance->mpLogger->warning("DebugLED FreeRTOS", "Can't delete Blink Timeout timer, because timer not exists.");
    }
}
void DebugLED::startBlinkTimeoutHandle(TimerHandle_t xTimer) {
    const DebugLED* instance = static_cast<DebugLED*>(pvTimerGetTimerID(xTimer));
    instance->blinkTimeoutCallback();
}
void DebugLED::startBlinkTimeout(const uint32_t maxBlinkTime) {
    if (msBlinkTimeout == nullptr) {
        msBlinkTimeout = xTimerCreate(
            "Blink Timeout",
            pdMS_TO_TICKS(maxBlinkTime),
            pdFALSE,
            nullptr,
            startBlinkTimeoutHandle
        );
    }
    
    if (msBlinkTimeout != nullptr) {
        xTimerStart(msBlinkTimeout, portMAX_DELAY);
    } else {
        mspInstance->mpLogger->error("DebugLED FreeRTOS", "Can't start Blink Timeout timer, because timer not exists.");
    }
}
void DebugLED::deleteBlinkTimeout() {
    if (msBlinkTimeout != nullptr) {
        xTimerDelete(msBlinkTimeout, portMAX_DELAY);
        msBlinkTimeout = nullptr;
    }
}
// ================================================================