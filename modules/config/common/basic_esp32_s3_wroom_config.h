#pragma once

#ifdef HC12_MODULE
    #include "hc12_common_config.h"

    #define HARDWARE_SERIAL_UART_NR 2
#endif

#define LED_PIN 1
#define BUTTON_PIN 39 // TODO !mm change pin: this is not RTC pin and can not wake up esp from deep sleep
#error "BUTTON_PIN_AS_GPIO is not defined"
#error "SECOND_WAKE_UP_PIN is not defined"
#error "BATTERY_PIN is not defined"