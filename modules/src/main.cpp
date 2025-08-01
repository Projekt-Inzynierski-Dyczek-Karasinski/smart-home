#include <Arduino.h>
#include <HardwareSerial.h>

#include "smart_home_config.h"
#include "universal_module_system/debug_led.h"
#include "universal_module_system/pairing_button.h"

#include "communication/communication.h"

void setup() {
    Serial.begin(9600);
    vTaskDelay(pdTICKS_TO_MS(1000));

    Serial.println(); 
    Serial.println("---FreeRTOS START---");
    DebugLED* debugLed = DebugLED::getInstance();
    
    Communication& communication = Communication::getInstance(debugLed);
    PairingButton* pairingButton = PairingButton::getInstance(debugLed, &communication);

    Serial.println("---setup() and loop() deleted---");
    vTaskDelete(NULL);
}

void loop() {}