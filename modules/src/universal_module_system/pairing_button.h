#ifndef PAIRING_BUTTON_H
#define PAIRING_BUTTON_H

#include <Arduino.h>
#include "debug_led.h"

class PairingButton {
    private:
        static void IRAM_ATTR buttonISR();
        static DebugLED *mspDebugLED;
        static uint8_t msButtonMode;

        static uint8_t msButtonPressCounter;
        static int8_t msButtonNotPressedCounter;
        static TimerHandle_t msButtonPressTimer;
        static void startButtonPressTimer();
        static void buttonPressTimerCallback();
        static void deleteButtonPressTimer();
        static void buttonPressTimerCallbackHandle(TimerHandle_t xTimer);

    public:
        PairingButton(DebugLED *debugLED);
        ~PairingButton();
};

#endif