#ifndef BASIC_ESP32_S3_WROOM_CONFIG_H
#define BASIC_ESP32_S3_WROOM_CONFIG_H
    #ifdef HC12
        #define BAUD_RATE 9600
        #define RX_PIN 6 
        #define TX_PIN 5
        #define SET_PIN 7
        #define HARDWARE_SERIAL_UART_NR 2
        
        #define CENTRAL_UNIT
    #endif

    #define LED_PIN 39
    #define BUTTON_PIN 1

#endif