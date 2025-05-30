#ifndef PAIRING_BUTTON_H
#define PAIRING_BUTTON_H

#include <Arduino.h>
#include "debug_led.h"

#define DEBOUNCING_TIME 50

class PairingButton {
    private:
        static void IRAM_ATTR buttonISR();
        static DebugLED *spDebugLED;

        uint8_t buttonPressCounter = 0;
        int8_t buttonNotPressedCounter = 3;
        TimerHandle_t buttonPressTimer;
        void startButtonPressTimer();
        void buttonPressTimerCallback();
        static void buttonPressTimerCallbackHandle(TimerHandle_t xTimer);
    public:
        PairingButton(DebugLED *debugLED);
        ~PairingButton();
};

#endif