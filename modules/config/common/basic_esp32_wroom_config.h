#pragma once

#ifdef HC12_MODULE
    #include "hc12_common_config.h"
    #define HARDWARE_SERIAL_UART_NR 2
#endif

// TODO !mm change pin: this is not RTC pin and can not wake up esp from deep sleep
#define LED_PIN 26
#define BUTTON_PIN 27
#define BUTTON_PIN_AS_GPIO GPIO_NUM_27
#define RF_MODULE_WAKE_UP_PIN GPIO_NUM_14
#define RF_MODULE_WAKE_UP_PIN_BITMASK 1ULL << RF_MODULE_WAKE_UP_PIN
#define BATTERY_PIN 12