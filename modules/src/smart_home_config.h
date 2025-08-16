#pragma once

#define BACKGROUND_TASK_PRIORITY 1
#define LOW_TASK_PRIORITY 2
#define MEDIUM_TASK_PRIORITY 3
#define HIGH_TASK_PRIORITY 4
#define CRITICAL_TASK_PRIORITY 5
#define HC12_MODULE

#ifndef TERMINAL_BAUD_RATE
    #define TERMINAL_BAUD_RATE 9600
#endif
#ifndef LOGGING_LEVEL
    #define LOGGING_LEVEL 3 // INFO
#endif

#ifdef ESP32_WROOM_BOARD_TYPE
    #if defined(ESP32_S3_WROOM_BOARD_TYPE) || defined(ESP32_YD_BOARD_TYPE)
        #error "Only one of ESP32_WROOM_BOARD_TYPE, ESP32_S3_WROOM_BOARD_TYPE and ESP32_YD_BOARD_TYPE can be defined."
    #endif
    #include "config/basic_esp32_wroom_config.h"
#endif

#ifdef ESP32_S3_WROOM_BOARD_TYPE
    #if defined(ESP32_WROOM_BOARD_TYPE) || defined(ESP32_YD_BOARD_TYPE)
        #error "Only one of ESP32_WROOM_BOARD_TYPE, ESP32_S3_WROOM_BOARD_TYPE and ESP32_YD_BOARD_TYPE can be defined."
    #endif
    #include "config/basic_esp32_s3_wroom_config.h"
#endif

#ifdef ESP32_YD_BOARD_TYPE
    #if defined(ESP32_WROOM_BOARD_TYPE) || defined(ESP32_S3_WROOM_BOARD_TYPE)
        #error "Only one of ESP32_WROOM_BOARD_TYPE, ESP32_S3_WROOM_BOARD_TYPE and ESP32_YD_BOARD_TYPE can be defined."
    #endif
    #include "config/basic_esp32_s3_wroom_config.h"
#endif

// TODO remove this before merge with main
#define COMMUNICATION_WITHOUT_SAVING_ADDRESSING