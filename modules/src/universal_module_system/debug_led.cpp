#include "debug_led.h"
#include "smart_home_config.h"

#define CONNECTION_BLINK_DELAY 500
#define RESET_BLINK_DELAY 100
// tmp value
#define MAX_CONNECTION_BLINK_TIME 15000
#define MAX_RESET_BLINK_TIME 3000

TaskHandle_t DebugLED::msPairingBlinkHandle = NULL;
TaskHandle_t DebugLED::msPesetBlinkHandle = NULL;;
TimerHandle_t DebugLED::msBlinkTimeout = NULL;

DebugLED::DebugLED() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
}

TaskHandle_t DebugLED::getConnectionBlinkHandle() {
    return msPairingBlinkHandle;
}

void DebugLED::pairingBlink() {
    for(;;) {
        digitalWrite(LED_PIN, HIGH);
        vTaskDelay(pdMS_TO_TICKS(CONNECTION_BLINK_DELAY));
        digitalWrite(LED_PIN, LOW);
        vTaskDelay(pdMS_TO_TICKS(CONNECTION_BLINK_DELAY));
    }
}

void DebugLED::createPairingBlinkTaskHandle(void *parameters) {
    DebugLED* instance = static_cast<DebugLED*>(parameters);
    instance->pairingBlink();
}

void DebugLED::createPairingBlinkTask() {
    if (msPairingBlinkHandle == NULL) {
        if (msPesetBlinkHandle != NULL) {
            Serial.println("void createConnectionBlinkTask() - Deletes Reset Blink task");
            deleteResetBlinkTask();
        }

        xTaskCreate(
            createPairingBlinkTaskHandle,
            "Connection Blink",
            1024,
            NULL,
            0,
            &msPairingBlinkHandle
        );
        startBlinkTimeout(MAX_CONNECTION_BLINK_TIME);
    } else {
        Serial.println("void createConnectionBlinkTask() - Can't create Connection Blink task -> Connection Blink task already exists");
    }
}

void DebugLED::deletePairingBlinkTask() {
    if (msPairingBlinkHandle != NULL) {
        if (msBlinkTimeout != NULL) {
            xTimerDelete(msBlinkTimeout, portMAX_DELAY);
            msBlinkTimeout = NULL;
        }

        vTaskDelete(msPairingBlinkHandle);
        msPairingBlinkHandle = NULL;
        digitalWrite(LED_PIN, LOW);
    }
}

void DebugLED::blinkTimeoutCallback() {
    if (msBlinkTimeout != NULL) {
        xTimerDelete(msBlinkTimeout, portMAX_DELAY);
        msBlinkTimeout = NULL;

        if (msPairingBlinkHandle != NULL) {
            deletePairingBlinkTask();
        }

        if (msPesetBlinkHandle != NULL) {
            deleteResetBlinkTask();
        }
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

void DebugLED::resetBlink() {
    for(;;) {
        digitalWrite(LED_PIN, HIGH);
        vTaskDelay(pdMS_TO_TICKS(RESET_BLINK_DELAY));
        digitalWrite(LED_PIN, LOW);
        vTaskDelay(pdMS_TO_TICKS(RESET_BLINK_DELAY));
    }
}

void DebugLED::createResetBlinkTaskHandle(void *parameters) {
    DebugLED* instance = static_cast<DebugLED*>(parameters);
    instance->resetBlink();
}

void DebugLED::createResetBlinkTask() {
    if (msPesetBlinkHandle == NULL) {
        if(msPairingBlinkHandle != NULL) {
            Serial.println("void createResetBlinkTask() - Deletes Connection Blink task");
            deletePairingBlinkTask();
        }
        xTaskCreate(
            createResetBlinkTaskHandle,
            "Reset Blink",
            1024,
            NULL,
            0,
            &msPesetBlinkHandle
        );
        startBlinkTimeout(MAX_RESET_BLINK_TIME);
    } else {
        Serial.println("void createResetBlinkTask() - Can't create Reset Blink task -> Reset Blink task already exists");
    }
}

void DebugLED::deleteResetBlinkTask() {
    if (msPesetBlinkHandle != NULL) {
        if (msBlinkTimeout != NULL) {
            xTimerDelete(msBlinkTimeout, portMAX_DELAY);
            msBlinkTimeout = NULL;
        }

        vTaskDelete(msPesetBlinkHandle);
        msPesetBlinkHandle = NULL;
        digitalWrite(LED_PIN, LOW);
    }
}

TaskHandle_t DebugLED::getResetBlinkHandle() {
    return msPesetBlinkHandle;
}

DebugLED::~DebugLED() {
    deletePairingBlinkTask();
    deleteResetBlinkTask();

    if (msBlinkTimeout != NULL) {
        xTimerDelete(msBlinkTimeout, portMAX_DELAY);
        msBlinkTimeout = NULL;
    }
    digitalWrite(LED_PIN, LOW);
}
