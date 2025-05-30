#include "debug_led.h"
#include "smart_home_config.h"

#define CONNECTION_BLINK_DELAY 500
#define RESET_BLINK_DELAY 100
// tmp value
#define MAX_CONNECTION_BLINK_TIME 5000
#define MAX_RESET_BLINK_TIME 1000

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
        startBlinkTimeout(MAX_CONNECTION_BLINK_TIME);
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

        if (resetBlinkHandle != NULL) {
            deleteResetBlinkTask();
        }
    } else {
        Serial.println("void blinkTimeoutCallback() - Can't delete blinkTimeout timer -> blinkTimeout timer not exists");
    }
}

void DebugLED::startBlinkTimeout(uint32_t maxBlinkTime) {
    if (blinkTimeout == NULL) {
        blinkTimeout = xTimerCreate(
            "Blink Timeout",
            pdMS_TO_TICKS(maxBlinkTime),
            pdFALSE,
            this,
            startBlinkTimeoutHandle
        );
    }
    
    if (blinkTimeout != NULL) {
        xTimerStart(blinkTimeout, portMAX_DELAY);
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
    if (resetBlinkHandle == NULL) {
        if(connectionBlinkHandle != NULL) {
            Serial.println("void createResetBlinkTask() - Deletes Connection Blink task");
            deleteConnectionBlinkTask();
        }
        xTaskCreate(
            createResetBlinkTaskHandle,
            "Reset Blink",
            1024,
            this,
            0,
            &resetBlinkHandle
        );
        startBlinkTimeout(MAX_RESET_BLINK_TIME);
    } else {
        Serial.println("void createResetBlinkTask() - Can't create Reset Blink task -> Reset Blink task already exists");
    }
}

void DebugLED::deleteResetBlinkTask() {
    if (resetBlinkHandle != NULL) {
        if (blinkTimeout != NULL) {
            xTimerDelete(blinkTimeout, portMAX_DELAY);
            blinkTimeout = NULL;
        }

        vTaskDelete(resetBlinkHandle);
        resetBlinkHandle = NULL;
        digitalWrite(LED_PIN, LOW);
    } else {
        Serial.println("void deleteResetBlinkTask() - Can't delete Reset Blink task -> Reset Blink task not exists");
    }
}

TaskHandle_t DebugLED::getResetBlinkHandle() {
    return resetBlinkHandle;
}

DebugLED::~DebugLED() {
    if (connectionBlinkHandle != NULL) {
        deleteConnectionBlinkTask();
    }

    if (resetBlinkHandle != NULL) {
        deleteResetBlinkTask();
    }

    if (blinkTimeout != NULL) {
        xTimerDelete(blinkTimeout, portMAX_DELAY);
        blinkTimeout = NULL;
    }
    digitalWrite(LED_PIN, LOW);
}
