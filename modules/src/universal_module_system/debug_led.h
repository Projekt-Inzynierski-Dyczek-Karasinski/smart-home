#ifndef DEBUG_LED_H
#define DEBUG_LED_H

#include <Arduino.h>

class DebugLED {
private:
    static TaskHandle_t msPairingBlinkHandle;
    static TaskHandle_t msPesetBlinkHandle;
    static TimerHandle_t msBlinkTimeout;

    static void pairingBlink();
    static void createPairingBlinkTaskHandle(void *parameters);

    static void resetBlink();
    static void createResetBlinkTaskHandle(void *parameters);

    static void startBlinkTimeout(uint32_t maxBlinkTime);
    static void blinkTimeoutCallback();
    static void startBlinkTimeoutHandle(TimerHandle_t xTimer);
    
public:
    DebugLED();
    
    static void createPairingBlinkTask();
    static void deletePairingBlinkTask();
    static TaskHandle_t getConnectionBlinkHandle();

    static void createResetBlinkTask();
    static void deleteResetBlinkTask();
    static TaskHandle_t getResetBlinkHandle();

    ~DebugLED();
};

#endif