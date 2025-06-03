#ifndef PAIRING_BUTTON_H
#define PAIRING_BUTTON_H

#include <Arduino.h>
#include "debug_led.h"

// TODO change class to singleton

/**
 * @brief Class that controls the Pairing Button by attaching interrupt to it. 
 * 
 * Pressing button for 3 seconds initializes a pairing process. 
 * 
 * Pressing button for 10 seconds initializes a reset process. 
 * @warning This class SHOULD BE initialized only once and destructor of this class SHOULD NEVER BE used. 
 * @note This class should be initialized at the very beginning of setup() (but after DebugLED class). Serial.begin() have to be initialized separately before this class to see debug messages.
 * 
 */
class PairingButton {
public:
    /**
     * @brief Constructor of PairingButton class. Sets BUTTON_PIN to INPUT_PULLUP and attaches interrupt to it.
     * @param DebugLED* Pointer to DebugLED object.
     */
    PairingButton(DebugLED *debugLED);
    
    /**
     * @brief Destructor of PairingButton class. Detaches interrupt from BUTTON_PIN and deletes Button Press Timer if exists.
     * @warning Destructor of this class exists only for programming principles. This class SHOULD NEVER BE deleted.
     */
    ~PairingButton();

private:
    /**
     * @brief Method that is called when the button is pressed (and interrupt is attached) and calls startButtonPressTimer().
     * @note This method detaches interrupt for debouncing reasons. Interrupt is reattached at the end of buttonPressTimerCallback().
     * 
     * This method is private.
     */
    static void IRAM_ATTR buttonISR();


    /**
     * @brief Method that is called when the Button Press Timer expires. It debounces a button and controls its logic. 
     * @note This method is private.
     */
    static void buttonPressTimerCallback();

    /**
     * @brief Method that makes static cast of PairingButton object.
     * @note This method exists only because is necessary for creating timer inside class in freeRTOS.
     * 
     * This method is private.
     * @param TimerHandle_t FreeRTOS software timer.
     */
    static void buttonPressTimerCallbackHandle(TimerHandle_t xTimer);

    /**
     * @brief Method that starts the Button Press Timer. If timer is already started, it will be restarted.
     * @note This method is private.
     */
    static void startButtonPressTimer();

    /**
     * @brief Method that deletes the Button Press Timer and resets button's variables to default values. 
     * If Button Press Timer doesn't exist, it only reset variables.
     * @note Button's variables are: msButtonPressCounter, msButtonNotPressedCounter.
     * 
     * This method is private.
     */
    static void deleteButtonPressTimer();


    static DebugLED *mspDebugLED;
    static uint8_t msButtonMode;
    static uint8_t msButtonPressCounter;
    static int8_t msButtonNotPressedCounter;
    static TimerHandle_t msButtonPressTimer;
};

#endif