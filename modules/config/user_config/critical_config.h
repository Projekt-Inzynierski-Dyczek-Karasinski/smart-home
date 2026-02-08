#pragma once
// TODO !pr check/add comments
// This is critical config data, that need to be set.
// BUTTON_PIN must be RTC_GPIO (pins 0 - 21), excluding GPIO0 and GPIO3.

// TODO consider moving here more constant config like hc12 pins

// TODO !pr remove #ifdefs
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
