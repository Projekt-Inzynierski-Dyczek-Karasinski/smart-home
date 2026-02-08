#pragma once
// TODO !pr check/add comments
// This is critical config data, that data must be set.

// TODO consider moving here more constant config like hc12 pins

// TODO !pr remove #ifdefs
// BUTTON_PIN must be RTC_GPIO (pins 0 - 21), excluding GPIO0 and GPIO3.
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

// RF module
#define HC12_MODULE

// Magic number, if not match with magic number in base_config.json, will start ota and disables ordinary execution
// of the program (usefully if code update is not compatible with old base_config.json or vice versa)
#define OTA_CHECK 2