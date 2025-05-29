#ifndef DEBUG_LED_H
#define DEBUG_LED_H

#include <Arduino.h>

class DebugLED {
private:
    // bool isOn;
    TaskHandle_t connectionBlinkHandle;
    TimerHandle_t blinkTimeout;

    void connectionBlink();
    static void createConnectionBlinkTaskHandle(void *parameters);
    void startBlinkTimeout();
    static void startBlinkTimeoutHandle(TimerHandle_t xTimer);
    void blinkTimeoutCallback();
    
public:
    DebugLED();
    ~DebugLED();
    
    void createConnectionBlinkTask();
    void deleteConnectionBlinkTask();
    TaskHandle_t getConnectionBlinkHandle();
};

#endif