// 18:00-19:00 + 30min
#include <Arduino.h>
#include <HardwareSerial.h>

#include "smart_home_config.h"

#include <freertos/message_buffer.h>
#if CONFIG_FREERTOS_UNICORE
    static const BaseType_t APP_CPU = 0;
#else
    static const BaseType_t APP_CPU = 1;
#endif

// #define DEBOUNCING_TIME 50
#define BUTTON_PRESS_TIME 50
#define BUTTON_PRESS_TIME_TO_SECOND(value) (value / (1000 / BUTTON_PRESS_TIME))
#define BUTTON_COUNTER_TO_UNLOCK 3
uint8_t buttonPressCounter = 0;
int8_t buttonNotPressedCounter = BUTTON_COUNTER_TO_UNLOCK;
static TimerHandle_t sButtonPressTimer = NULL;

static TaskHandle_t blinkLedHandle = NULL;
void buttonISR();

void blinkLed(void *parameters) {
    uint32_t notificationValue = 0;
    for(;;) {
        if (xTaskNotifyWait(0, ULONG_MAX, &notificationValue, portMAX_DELAY) == pdTRUE) {
            digitalWrite(LED_PIN, HIGH);
            vTaskDelay(pdMS_TO_TICKS(100));
            digitalWrite(LED_PIN, LOW);
        }
    }
}

void buttonPressTimerCallback(TimerHandle_t xTimer) {
    if (digitalRead(BUTTON_PIN) == LOW) {
        buttonPressCounter++;
        buttonNotPressedCounter = BUTTON_COUNTER_TO_UNLOCK;
        xTaskNotify(blinkLedHandle, 0, eSetValueWithOverwrite);
    } else {
        buttonNotPressedCounter--;

        if (buttonNotPressedCounter <= 0) {
            xTimerStop(sButtonPressTimer, portMAX_DELAY);

            Serial.printf("Button pressed for %d seconds\n", BUTTON_PRESS_TIME_TO_SECOND(buttonPressCounter));

            buttonNotPressedCounter = 3;
            buttonPressCounter = 0;
            Serial.println("Button released, interrupt attached");
            attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);
        }
    }
}

void IRAM_ATTR buttonISR() {
    detachInterrupt(digitalPinToInterrupt(BUTTON_PIN));
    if (xTimerStart(sButtonPressTimer, pdTICKS_TO_MS(100)) == pdFAIL) {
        attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);
        Serial.println("Debouncing timer failed to start in buttonISR()!!!");
    }
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    Serial.println("Button pressed");

    // Force context switch if higher priority task was woken
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void setup() {
    Serial.begin(9600);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    vTaskDelay(pdTICKS_TO_MS(1000));
    Serial.println(); 
    Serial.println("---FreeRTOS START---");

    sButtonPressTimer = xTimerCreate(
        "Button Press Timer",
        pdMS_TO_TICKS(BUTTON_PRESS_TIME),
        pdTRUE,
        NULL,
        buttonPressTimerCallback
    );

    xTaskCreatePinnedToCore(
        blinkLed,
        "Blink Led",
        1024,
        NULL,
        0,
        &blinkLedHandle,
        APP_CPU
    );
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);

    vTaskDelete(NULL);
}

void loop() {}