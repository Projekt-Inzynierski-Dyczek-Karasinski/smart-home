#pragma once

#ifdef DEVELOPMENT_MODE
    // disable wake up rf notifications that are annoying during software development
    #define DISABLE_WAKE_UP_RF_NOTIFICATION
    // enable terminal input and change default config to INFO
    #define DEBUG_MODE

    #ifdef ESP32_S3_BOARD_TYPE
    // change features module specific features to central unit features
        #define CENTRAL_UNIT
    #endif

    // ESP32 devboard specific config
    // remove pins set in critical_config.h
    #ifdef LED_PIN
        #warning "#undef LED_PIN"
        #undef LED_PIN
    #endif
    #ifdef BUTTON_PIN
        #warning "#undef BUTTON_PIN"
        #undef BUTTON_PIN
    #endif

    #ifdef ESP32_S3_BOARD_TYPE
    #ifdef CENTRAL_UNIT
        #define LED_PIN 39
        #define BUTTON_PIN 1
    #else
        #define LED_PIN 48
        #define BUTTON_PIN 2
    #endif
    #else
    // TODO !mm change pin: this is not RTC pin and can not wake up esp from deep sleep
        #define LED_PIN 26
        #define BUTTON_PIN 27
    #endif

#endif
