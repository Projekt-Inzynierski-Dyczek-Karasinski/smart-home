#ifndef SMART_HOME_CONFIG_H
#define SMART_HOME_CONFIG_H
    #define BACKGROUND_TASK_PRIORITY 1
    #define LOW_TASK_PRIORITY 2
    #define MEDIUM_TASK_PRIORITY 3
    #define HIGH_TASK_PRIORITY 4
    #define CRITICAL_TASK_PRIORITY 5


    #define HC12


    #ifdef ESP32_WROOM_BOARD_TYPE
        #include "config/basic_esp32_wroom_config.h"
    #endif
    #ifdef ESP32_S3_WROOM_BOARD_TYPE
        #include "config/basic_esp32_s3_wroom_config.h"
    #endif
#endif