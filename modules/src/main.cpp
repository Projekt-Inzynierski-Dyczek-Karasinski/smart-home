#include <Arduino.h>
#include <HardwareSerial.h>

#include "smart_home_config.h"
#include "universal_module_system/debug_led.h"
#include "universal_module_system/pairing_button.h"
#include "communication/communication.h"

// TODO remove
// HardwareSerial mspSerial(HARDWARE_SERIAL_UART_NR);
void setup() {
    vTaskDelay(pdTICKS_TO_MS(1000));
    Serial.begin(9600);
    Serial.println(); 
    Serial.println("---FreeRTOS START---");
    
    DebugLED debugLed;
    Communication communication(&debugLed);
    PairingButton pairingButton(&debugLed, &communication);
    // TODO remove
    // mspSerial.begin((unsigned long)BAUD_RATE, SERIAL_8N1, RX_PIN, TX_PIN);

    Serial.println("---setup() and loop() deleted---");
    vTaskDelete(NULL);
}

void loop() {
    // TODO remove
    // while (Serial.available() > 0) {
    //     mspSerial.write(Serial.read());
    // }
    // while (mspSerial.available() > 0) {
    //     Serial.write(mspSerial.read());
    // }
}