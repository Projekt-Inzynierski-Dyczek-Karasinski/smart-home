#pragma once

#include "../config/user_config/critical_config.h"
#include "../config/user_config/optional_config.h"

// debug
#ifdef DEBUG_MODE
    // #define TEST_CHECKSUM
#endif

#ifdef HC12_MODULE
    #include "hc12_common_config.h"
#else
    #error "Not implemented"
#endif

#include "freertos_common_config.h"

// Board config
#ifdef ESP32_WROOM_BOARD_TYPE
    #if defined(ESP32_S3_BOARD_TYPE)
        #error "Only one of ESP32_WROOM_BOARD_TYPE, ESP32_S3_WROOM_BOARD_TYPE and ESP32_S3_BOARD_TYPE can be defined."
    #endif
#endif

#ifdef ESP32_S3_BOARD_TYPE
    #ifdef DEBUG_MODE
        #define CENTRAL_UNIT // TODO !mm remove/comment
    #endif

    #if defined(ESP32_WROOM_BOARD_TYPE)
        #error "Only one of ESP32_WROOM_BOARD_TYPE, ESP32_S3_WROOM_BOARD_TYPE and ESP32_S3_BOARD_TYPE can be defined."
    #endif
#endif