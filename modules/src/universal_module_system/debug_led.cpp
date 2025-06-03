#include "debug_led.h"
#include "smart_home_config.h"

#define CONNECTION_BLINK_DELAY 500
#define RESET_BLINK_DELAY 100

// TODO assign final value
#define MAX_CONNECTION_BLINK_TIME 15000
// TODO assign final value
#define MAX_RESET_BLINK_TIME 3000

TaskHandle_t DebugLED::msPairingBlinkHandle = NULL;
TaskHandle_t DebugLED::msResetBlinkHandle = NULL;;
TimerHandle_t DebugLED::msBlinkTimeout = NULL;


DebugLED::DebugLED() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
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

void DebugLED::blink(uint32_t ledOnDuration, uint32_t ledOffDuration) {
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
    DebugLED* instance = static_cast<DebugLED*>(parameters);
    instance->pairingBlink();
}
void DebugLED::createPairingBlinkTask() {
    if (msPairingBlinkHandle == NULL) {
        deleteResetBlinkTask();

        xTaskCreate(
            createPairingBlinkTaskHandle,
            "Pairing Blink",
            1024,
            NULL,
            0,
            &msPairingBlinkHandle
        );
    } else {
        Serial.println("void createConnectionBlinkTask() - Can't create Connection Blink task -> Connection Blink task already exists");
    }
    startBlinkTimeout(MAX_CONNECTION_BLINK_TIME);
}
void DebugLED::deletePairingBlinkTask() {
    if (msPairingBlinkHandle != NULL) {
        deleteBlinkTimeout();
        
        vTaskDelete(msPairingBlinkHandle);
        msPairingBlinkHandle = NULL;
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
    DebugLED* instance = static_cast<DebugLED*>(parameters);
    instance->resetBlink();
}
void DebugLED::createResetBlinkTask() {
    if (msResetBlinkHandle == NULL) {
        deletePairingBlinkTask();
        
        xTaskCreate(
            createResetBlinkTaskHandle,
            "Reset Blink",
            1024,
            NULL,
            0,
            &msResetBlinkHandle
        );
        startBlinkTimeout(MAX_RESET_BLINK_TIME);
    } else {
        Serial.println("void createResetBlinkTask() - Can't create Reset Blink task -> Reset Blink task already exists");
    }
}
void DebugLED::deleteResetBlinkTask() {
    if (msResetBlinkHandle != NULL) {
        deleteBlinkTimeout();
        
        vTaskDelete(msResetBlinkHandle);
        msResetBlinkHandle = NULL;
        digitalWrite(LED_PIN, LOW);
    }
}
// ================================================================

// ===================== Blink Timeout Timer ======================

void DebugLED::blinkTimeoutCallback() {
    if (msBlinkTimeout != NULL) {
        if (msPairingBlinkHandle != NULL) {
            deletePairingBlinkTask();
        }
        
        if (msResetBlinkHandle != NULL) {
            deleteResetBlinkTask();
        }

        deleteBlinkTimeout();
    } else {
        Serial.println("void blinkTimeoutCallback() - Can't delete blinkTimeout timer -> blinkTimeout timer not exists");
    }
}
void DebugLED::startBlinkTimeoutHandle(TimerHandle_t xTimer) {
    DebugLED* instance = static_cast<DebugLED*>(pvTimerGetTimerID(xTimer));
    instance->blinkTimeoutCallback();
}
void DebugLED::startBlinkTimeout(uint32_t maxBlinkTime) {
    if (msBlinkTimeout == NULL) {
        msBlinkTimeout = xTimerCreate(
            "Blink Timeout",
            pdMS_TO_TICKS(maxBlinkTime),
            pdFALSE,
            NULL,
            startBlinkTimeoutHandle
        );
    }
    
    if (msBlinkTimeout != NULL) {
        xTimerStart(msBlinkTimeout, portMAX_DELAY);
    } else {
        Serial.println("void startBlinkTimeout() - Can't create blinkTimeout timer -> blinkTimeout timer not created");
    }
}
void DebugLED::deleteBlinkTimeout() {
    if (msBlinkTimeout != NULL) {
        xTimerDelete(msBlinkTimeout, portMAX_DELAY);
        msBlinkTimeout = NULL;
    }
}
// ================================================================