#include <Arduino.h>

#include "smart_home_config.h"
#include "universal_module_system/debug_led.h"
#include "universal_module_system/pairing_button.h"

// #if CONFIG_FREERTOS_UNICORE
//     static const BaseType_t APP_CPU = 0;
// #else
//     static const BaseType_t APP_CPU = 1;
// #endif
// static TimerHandle_t sButtonPressTimer = NULL;


// void IRAM_ATTR buttonISR() {
//     detachInterrupt(digitalPinToInterrupt(BUTTON_PIN));
    
//     BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
//     Serial.println("Button pressed");
//     if (debugLed.getConnectionBlinkHandle() == NULL) {
//         debugLed.createConnectionBlinkTask();
//     } else {
//         debugLed.deleteConnectionBlinkTask();
//     }

//     xTimerStart(sButtonPressTimer, portMAX_DELAY);

//     // Force context switch if higher priority task was woken
//     portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
// }

// void buttonPressTimerCallback(TimerHandle_t xTimer) {
//     attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);
// }



void setup() {
    Serial.begin(9600);
    // pinMode(BUTTON_PIN, INPUT_PULLUP);
    // pinMode(LED_PIN, OUTPUT);
    // digitalWrite(LED_PIN, LOW);
    DebugLED debugLed;
    PairingButton pairingButton(&debugLed);

    vTaskDelay(pdTICKS_TO_MS(1000));
    Serial.println(); 
    Serial.println("---FreeRTOS START---");

    // sButtonPressTimer = xTimerCreate(
    //     "Button Press Timer",
    //     pdMS_TO_TICKS(1000),
    //     pdFALSE,
    //     NULL,
    //     buttonPressTimerCallback
    // );

    // attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);

    vTaskDelete(NULL);
}

void loop() {}