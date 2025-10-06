#pragma once

#ifdef HC12_MODULE
    #include "hc12_common_config.h"

    #define BAUD_RATE 9600
    #define RX_PIN 6 
    #define TX_PIN 7
    #define SET_PIN 5
    #define HARDWARE_SERIAL_UART_NR 2
#endif

#define LED_PIN 1
#define BUTTON_PIN 39