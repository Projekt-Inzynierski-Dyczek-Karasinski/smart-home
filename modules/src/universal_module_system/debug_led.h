#ifndef DEBUG_LED_H
#define DEBUG_LED_H

#include <Arduino.h>
class DebugLED {
private:
    // bool isOn;
    void connectionBlink();
    static void createConnectionBlinkTaskHandle(void *parameters);
    TaskHandle_t connectionBlinkHandle;
    
public:
    DebugLED();
    ~DebugLED();
    
    void createConnectionBlinkTask();
    void deleteConnectionBlinkTask();
    TaskHandle_t getConnectionBlinkHandle();
};

#endif