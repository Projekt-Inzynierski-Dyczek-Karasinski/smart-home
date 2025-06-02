#ifndef PAIRING_BUTTON_H
#define PAIRING_BUTTON_H

#include <Arduino.h>
#include "debug_led.h"

class PairingButton {
private:
    static DebugLED *mspDebugLED;
    static uint8_t msButtonMode;
    static uint8_t msButtonPressCounter;
    static int8_t msButtonNotPressedCounter;
    static TimerHandle_t msButtonPressTimer;

    /**
     * @brief Method that is called when the Button Press Timer expires. It debounces a button and controls its logic. 
     */
    static void buttonPressTimerCallback();

    /**
     * @brief Method that starts the Button Press Timer. If timer is already started, it will be restarted.
     */
    static void startButtonPressTimer();

    /**
     * @brief Method that deletes the Button Press Timer and resets button's variables to default values. 
     * If Button Press Timer doesn't exist, it only reset variables.
     * @note Button's variables are: msButtonPressCounter, msButtonNotPressedCounter.
     */
    static void deleteButtonPressTimer();

    /**
     * @brief Method that makes static cast of PairingButton object.
     * @note This method exists only because is necessary for creating timer inside class in freeRTOS.
     * @param TimerHandle_t FreeRTOS software timer.
     */
    static void buttonPressTimerCallbackHandle(TimerHandle_t xTimer);

    /**
     * @brief Method that is called when the button is pressed (and interrupt is attached) and calls startButtonPressTimer().
     * @note This method detaches interrupt for debouncing reasons. Interrupt is reattached at the end of buttonPressTimerCallback().
     */
    static void IRAM_ATTR buttonISR();
public:
    /**
     * @brief Constructor of PairingButton class. Sets BUTTON_PIN to INPUT_PULLUP and attaches interrupt to it.
     * @param DebugLED* Pointer to DebugLED object.
     */
    PairingButton(DebugLED *debugLED);
    
    /**
     * @brief Destructor of PairingButton class. Detaches interrupt from BUTTON_PIN and deletes Button Press Timer if exists.
     * @warning Destructor of this class exists only for programming principles. This class SHOULD BE NEVER deleted.
     */
    ~PairingButton();
};

#endif