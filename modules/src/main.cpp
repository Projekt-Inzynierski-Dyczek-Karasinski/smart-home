#include <Arduino.h>
#include <HardwareSerial.h>

#include "smart_home_config.h"
#include "universal_module_system/debug_led.h"
#include "universal_module_system/pairing_button.h"

#include "communication/communication.h"

void setup() {
    vTaskDelay(pdMS_TO_TICKS(1000));
    Serial.begin(9600);
    Serial.println(); 
    Serial.println("---FreeRTOS START---");
    
    DebugLED debugLed;
    Communication& communication = Communication::getInstance(&debugLed);
    PairingButton pairingButton(&debugLed, &communication);

    Serial.println("---setup() and loop() deleted---");
    vTaskDelete(NULL);
}

void loop() {}