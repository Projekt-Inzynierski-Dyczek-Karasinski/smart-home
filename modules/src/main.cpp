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
    // uint8_t mac[6];
    // char mac2[6];
    // esp_read_mac(mac, ESP_MAC_WIFI_STA);

    // Serial.print("MAC address: ");
    // for (uint8_t num : mac){
    //     Serial.print(num);
    // }
    // Serial.println();
    // for (int i=0;i<6;i++){
    //     mac2[i]=mac[i];
    // }
    // Serial.print("MAC addres2: ");
    // Serial.println(mac2);


    vTaskDelete(NULL);
}

void loop() {}