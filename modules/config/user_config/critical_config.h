#pragma once
// This is critical config data; it must be set

// TODO consider moving here more constant config like hc12 pins

// BUTTON_PIN must be RTC_GPIO (pins 0 - 21), excluding GPIO0 and GPIO3.
#define BUTTON_PIN 2

// Pin where the LED is connected
#define LED_PIN 48

// RF module
#define HC12_MODULE

/*
 Version number indicating which base_config.json is compatible with this code.
 If it does not match the value in base_config.json, the device will start OTA and disable normal program execution.
 Useful when a code update isn’t compatible with an older base_config.json, or vice versa.
*/
#define CONFIG_COMPAT_VERSION 5
