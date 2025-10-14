#pragma once
#ifdef HC12_MODULE
    #include "hc12_common_config.h"

#define BAUD_RATE 9600
    #define RX_PIN 6
    #define TX_PIN 7
    #define SET_PIN 5
    #define HARDWARE_SERIAL_UART_NR 2
#endif

#define LED_PIN 39
#define BUTTON_PIN 1

#define BUTTON_PIN_AS_GPIO GPIO_NUM_1
#define RF_MODULE_WAKE_UP_PIN GPIO_NUM_15
#define RF_MODULE_WAKE_UP_PIN_BITMASK 1ULL << RF_MODULE_WAKE_UP_PIN
#define BATTERY_PIN 16

#define CENTRAL_UNIT