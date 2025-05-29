#include <Arduino.h>
#include "smart_home_config.h"
#include "debug_led.h"

#define CONNECTION_BLINK_DELAY 500

DebugLED::DebugLED() {
    pinMode(LED_PIN, OUTPUT);
    // isOn = false;
    connectionBlinkHandle = NULL;
}

DebugLED::~DebugLED() {
    deleteConnectionBlinkTask();
    digitalWrite(LED_PIN, LOW);
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
    } else {
        Serial.println("void createConnectionBlinkTask() - Can't create Connection Blink task -> Connection Blink task already exists");
    }
}

void DebugLED::deleteConnectionBlinkTask() {
    if (connectionBlinkHandle != NULL) {
        vTaskDelete(connectionBlinkHandle);
        connectionBlinkHandle = NULL;
        digitalWrite(LED_PIN, LOW);
    } else {
        Serial.println("void deleteConnectionBlinkTask() - Can't delete Connection Blink task -> Connection Blink task not exists");
    }
}