#pragma once

#ifdef HC12_MODULE
    #include "hc12_common_config.h"

    #define BAUD_RATE 9600
    #define RX_PIN 25 
    #define TX_PIN 33
    #define SET_PIN 32
    #define HARDWARE_SERIAL_UART_NR 2
#endif

#define LED_PIN 26
#define BUTTON_PIN 27

// #define CENTRAL_UNIT
