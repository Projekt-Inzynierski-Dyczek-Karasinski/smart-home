#pragma once

#define BACKGROUND_TASK_PRIORITY 1
#define LOW_TASK_PRIORITY 2
#define MEDIUM_TASK_PRIORITY 3
#define HIGH_TASK_PRIORITY 4
#define CRITICAL_TASK_PRIORITY 5
#define HC12_MODULE

#if defined(ESP32_WROOM_BOARD_TYPE) && defined(ESP32_S3_WROOM_BOARD_TYPE) 
    #error "Only one of ESP32_WROOM_BOARD_TYPE and ESP32_S3_WROOM_BOARD_TYPE can be defined."
#endif

#ifdef ESP32_WROOM_BOARD_TYPE
    #include "config/basic_esp32_wroom_config.h"
#endif

#ifdef ESP32_S3_WROOM_BOARD_TYPE
    #include "config/basic_esp32_s3_wroom_config.h"
#endif
