#pragma once

// TODO before merge with main comment/remove DEBUG_MODE
#define DEBUG_MODE

// RF module
#define HC12_MODULE

// Default baud rate for printing
#ifndef TERMINAL_BAUD_RATE
    #define TERMINAL_BAUD_RATE 9600
#endif

// Logger
#ifndef LOGGING_LEVEL
    #ifdef DEBUG_MODE
        #define LOGGING_LEVEL 3 // INFO
    #else
        #define LOGGING_LEVEL 0 // NONE
    #endif
#endif

#include "config/freertos_config.h"

// Board config
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

// TODO before merge with main remove COMMUNICATION_WITHOUT_SAVING_ADDRESSING
// #define COMMUNICATION_WITHOUT_SAVING_ADDRESSING