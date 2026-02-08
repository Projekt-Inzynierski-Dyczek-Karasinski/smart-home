#pragma once

//TODO !mm move  LED_PIN and BUTTON_PIN defines to base_config.json
#ifdef HC12_MODULE
    #include "hc12_common_config.h"
    #define CENTRAL_UNIT //TODO !mm move comment/remove
    #define HARDWARE_SERIAL_UART_NR 2
#endif

