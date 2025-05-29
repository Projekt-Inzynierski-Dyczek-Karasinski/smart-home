#include <Arduino.h>
#include "smart_home_config.h"
#include "debug_led.h"

#define CONNECTION_BLINK_DELAY 500
// tmp value
#define MAX_BLINK_TIME 5000

DebugLED::DebugLED() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    connectionBlinkHandle = NULL;
    blinkTimeout = NULL;
}


TaskHandle_t DebugLED::getConnectionBlinkHandle() {
    return connectionBlinkHandle;
}

void DebugLED::connectionBlink() {
    for(;;) {
        digitalWrite(LED_PIN, HIGH);
        vTaskDelay(pdMS_TO_TICKS(CONNECTION_BLINK_DELAY));
        digitalWrite(LED_PIN, LOW);
        vTaskDelay(pdMS_TO_TICKS(CONNECTION_BLINK_DELAY));
    }
}

void DebugLED::createConnectionBlinkTaskHandle(void *parameters) {
    DebugLED* instance = static_cast<DebugLED*>(parameters);
    instance->connectionBlink();
}

void DebugLED::createConnectionBlinkTask() {
    if (connectionBlinkHandle == NULL) {
        xTaskCreate(
            createConnectionBlinkTaskHandle,
            "Connection Blink",
            1024,
            this,
            0,
            &connectionBlinkHandle
        );
        startBlinkTimeout();
    } else {
        Serial.println("void createConnectionBlinkTask() - Can't create Connection Blink task -> Connection Blink task already exists");
    }
}

void DebugLED::deleteConnectionBlinkTask() {
    if (connectionBlinkHandle != NULL) {
        if (blinkTimeout != NULL) {
            xTimerDelete(blinkTimeout, portMAX_DELAY);
            blinkTimeout = NULL;
        }

        vTaskDelete(connectionBlinkHandle);
        connectionBlinkHandle = NULL;
        digitalWrite(LED_PIN, LOW);
    } else {
        Serial.println("void deleteConnectionBlinkTask() - Can't delete Connection Blink task -> Connection Blink task not exists");
    }
}

void DebugLED::startBlinkTimeoutHandle(TimerHandle_t xTimer) {
    DebugLED* instance = static_cast<DebugLED*>(pvTimerGetTimerID(xTimer));
    instance->blinkTimeoutCallback();
}

void DebugLED::blinkTimeoutCallback() {
    if (blinkTimeout != NULL) {
        xTimerDelete(blinkTimeout, portMAX_DELAY);
        blinkTimeout = NULL;

        if (connectionBlinkHandle != NULL) {
            deleteConnectionBlinkTask();
        }
    } else {
        Serial.println("void blinkTimeoutCallback() - Can't delete blinkTimeout timer -> blinkTimeout timer not exists");
    }
}

void DebugLED::startBlinkTimeout() {
    blinkTimeout = xTimerCreate(
        "Blink Timeout",
        pdMS_TO_TICKS(MAX_BLINK_TIME),
        pdFALSE,
        this,
        startBlinkTimeoutHandle
    );

    if (blinkTimeout != NULL) {
        xTimerStart(blinkTimeout, portMAX_DELAY);
    } else {
        Serial.println("void startBlinkTimeout() - Can't create blinkTimeout timer -> blinkTimeout timer not created");
    }
}

DebugLED::~DebugLED() {
    if (connectionBlinkHandle != NULL) {
        deleteConnectionBlinkTask();
    }

    if (blinkTimeout != NULL) {
        xTimerDelete(blinkTimeout, portMAX_DELAY);
        blinkTimeout = NULL;
    }
    digitalWrite(LED_PIN, LOW);
}
