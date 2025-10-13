#pragma once

// debug
// TODO before merge with main comment/remove DEBUG_MODE
#define DEBUG_MODE
#ifdef DEBUG_MODE
    // TODO before merge with main remove COMMUNICATION_WITHOUT_SAVING_ADDRESSING
    // #define COMMUNICATION_WITHOUT_SAVING_ADDRESSING
    // #define TEST_CHECKSUM
#endif

// RF module
#define HC12_MODULE

#include "freertos_common_config.h"

// Board config
#ifdef ESP32_WROOM_BOARD_TYPE
    #if defined(ESP32_S3_WROOM_BOARD_TYPE) || defined(ESP32_YD_BOARD_TYPE)
        #error "Only one of ESP32_WROOM_BOARD_TYPE, ESP32_S3_WROOM_BOARD_TYPE and ESP32_YD_BOARD_TYPE can be defined."
    #endif
    #include "basic_esp32_wroom_config.h"
#endif

#ifdef ESP32_S3_WROOM_BOARD_TYPE
    #if defined(ESP32_WROOM_BOARD_TYPE) || defined(ESP32_YD_BOARD_TYPE)
        #error "Only one of ESP32_WROOM_BOARD_TYPE, ESP32_S3_WROOM_BOARD_TYPE and ESP32_YD_BOARD_TYPE can be defined."
    #endif
    #include "basic_esp32_s3_wroom_config.h"
#endif

#ifdef ESP32_YD_BOARD_TYPE
    #if defined(ESP32_WROOM_BOARD_TYPE) || defined(ESP32_S3_WROOM_BOARD_TYPE)
        #error "Only one of ESP32_WROOM_BOARD_TYPE, ESP32_S3_WROOM_BOARD_TYPE and ESP32_YD_BOARD_TYPE can be defined."
    #endif
    #include "basic_esp32_yd_config.h"
#endif

// module config
// TODO consider saving that in SPIFFS:
// #define AUTO_SLEEP