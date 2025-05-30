#ifndef DEBUG_LED_H
#define DEBUG_LED_H

#include <Arduino.h>

class DebugLED {
private:
    // bool isOn;
    TaskHandle_t connectionBlinkHandle;
    TaskHandle_t resetBlinkHandle;
    TimerHandle_t blinkTimeout;

    void connectionBlink();
    static void createConnectionBlinkTaskHandle(void *parameters);

    void resetBlink();
    static void createResetBlinkTaskHandle(void *parameters);

    void startBlinkTimeout(uint32_t maxBlinkTime);
    void blinkTimeoutCallback();
    static void startBlinkTimeoutHandle(TimerHandle_t xTimer);
    
public:
    DebugLED();
    
    void createConnectionBlinkTask();
    void deleteConnectionBlinkTask();
    TaskHandle_t getConnectionBlinkHandle();

    void createResetBlinkTask();
    void deleteResetBlinkTask();
    TaskHandle_t getResetBlinkHandle();

    ~DebugLED();
};

#endif