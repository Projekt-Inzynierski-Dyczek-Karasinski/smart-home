#ifndef BASIC_ESP32_WROOM_CONFIG_H
#define BASIC_ESP32_WROOM_CONFIG_H
    #ifdef HC12
        #define BAUD_RATE 9600
        #define RX_PIN 25 
        #define TX_PIN 33
        #define SET_PIN 32
        #define HARDWARE_SERIAL_UART_NR 2
    #endif

    #define LED_PIN 26
    #define BUTTON_PIN 27
    
#endif