#pragma once

//TODO !mm move  LED_PIN and BUTTON_PIN defines to base_config.json
#ifdef HC12_MODULE
    #include "hc12_common_config.h"
    #define CENTRAL_UNIT //TODO !mm move comment/remove
    #define HARDWARE_SERIAL_UART_NR 2
#endif

#define LED_PIN 39
#define BUTTON_PIN 1
#define BUTTON_PIN_AS_GPIO GPIO_NUM_1
#define RF_MODULE_WAKE_UP_PIN GPIO_NUM_15

#define RF_MODULE_WAKE_UP_PIN_BITMASK 1ULL << RF_MODULE_WAKE_UP_PIN