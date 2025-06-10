#include <Arduino.h>
#include <HardwareSerial.h>

#include "smart_home_config.h"
#include "universal_module_system/debug_led.h"
#include "universal_module_system/pairing_button.h"
#include "communication/communication.h"
    

void setup() {
    Serial.begin(9600);
    DebugLED debugLed;
    PairingButton pairingButton(&debugLed);

    Communication communication;

    vTaskDelay(pdTICKS_TO_MS(1000));
    Serial.println(); 
    Serial.println("---FreeRTOS START---");

    vTaskDelete(NULL);
}

void loop() {}