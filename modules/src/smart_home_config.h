#ifndef SMART_HOME_CONFIG_H
#define SMART_HOME_CONFIG_H
    #define HC12


    #ifdef ESP32_WROOM_BOARD_TYPE
        #include "config/basic_esp32_wroom_config.h"
    #endif
    #ifdef ESP32_S3_WROOM_BOARD_TYPE
        #include "config/basic_esp32_s3_wroom_config.h"
    #endif
#endif