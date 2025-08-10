#pragma once

#include <Arduino.h>
#include "utils/logger.h"

// TODO change to thread-save singleton and change some methods and vars to nonstatic
namespace ul = Utils::Logging;

/**
 * @brief Class that controls the LED. 
 * 
 * Blinking slowly (0.5s ON, 0.5s OFF) indicates pairing process.
 * 
 * Blinking quickly (0.1s ON, 0.1s OFF) indicates reset process.
 * 
 * @warning This class must be initialized only once and destructor of this class should never be used. 
 * @note This class should be initialized at the very beginning of setup(). Serial.begin() have to be initialized separately before this class to see debug messages.
 * This class is a singleton.
 * 
 */
class DebugLED {
public:
    /**
     * @brief Method that initializes DebugLED and returns a pointer to the instance of DebugLED.
     * @return DebugLED* pointer to the instance of DebugLED.
     */
    static DebugLED* getInstance();
    
    // Delete copy constructor and assignment operator
    DebugLED(const DebugLED&) = delete;
    DebugLED& operator=(const DebugLED&) = delete;

    /**
     * @brief Getter returning the value of the handle for the Pairing Blink Task.
     * @return TaskHandle_t Handle to the Reset Blink Task, or NULL if the task is not exist.
     */
    static TaskHandle_t getPairingBlinkHandle();

    /**
     * @brief Getter returning the value of the handle for the Reset Blink Task.
     * @return TaskHandle_t Handle to the Reset Blink Task, or NULL if the task is not exist.
     */
    static TaskHandle_t getResetBlinkHandle();

    /**
     * @brief Creates Pairing Blink Task and starts Blink Timeout Timer. If the task is already exist, only starts timer.
     * @warning If Reset Blink Task exist, it will be deleted before creating Pairing, only one of them is allowed at the same time.
     */
    static void createPairingBlinkTask();

    /**
     * @brief Deletes Pairing Blink Task and deletes Blink Timeout Timer if exists.
     */
    static void deletePairingBlinkTask();
    
    /**
     * @brief Creates Reset Blink Task and starts Blink Timeout Timer. If the task is already exist, only starts timer.
     * @warning If Pairing Blink Task exist, it will be deleted before creating Reset, only one of them is allowed at the same time.
     */
    static void createResetBlinkTask();

    /**
     * @brief Deletes Pairing Blink Task and deletes Blink Timeout Timer if exists.
     */
    static void deleteResetBlinkTask();
    
private:
    /**
     * @brief Constructor of DebugLED class. Sets LED_PIN to OUTPUT and its state to LOW.
     * @note Constructor of this class is private, because this class is a singleton.
     */
    explicit DebugLED();

    /**
     * @brief Destructor of DebugLED class. Deletes all class's tasks and timers.
     * @warning Destructor of this class exists only for programming principles. This class should never be deleted.
     * @note Destructor of this class is private, because this class is a singleton.
     */
    ~DebugLED();

    /**
     * @brief Make LED blink for a given times.
     * @param ledOnDuration Time in milliseconds for which the LED will be on.
     * @param ledOffDuration Time in milliseconds for which the LED will be off.
     * @note This method is private.
     */
    static void blink(uint32_t ledOnDuration, uint32_t ledOffDuration);

    /**
     * @brief FreeRTOS task that blinks LED signalizing pairing process.
     * @note This method is private.
     */
    static void pairingBlink();

    /**
     * @brief Method that makes static cast of DebugLED object.
     * @note This method exists only because is necessary for creating task inside class in freeRTOS.
     * 
     * This method is private.
     * @param parameters FreeRTOS task parameters.
     */
    static void createPairingBlinkTaskHandle(void *parameters);

    /**
     * @brief FreeRTOS task that blinks LED signalizing reset process.
     * @note This method is private.
     */
    static void resetBlink();

    /**
     * @brief Method that makes static cast of DebugLED object.
     * @note This method exists only because is necessary for creating task inside class in freeRTOS.
     * 
     * This method is private.
     * @param parameters FreeRTOS task parameters.
     */
    static void createResetBlinkTaskHandle(void *parameters);

    /**
     * @brief Method that is called when the Blink Timeout Timer expires. Deletes Timer and Pairing Blink and Reset Blink tasks if exists.
     * @note This method is private.
     */
    static void blinkTimeoutCallback();

    /**
     * @brief Method that makes static cast of DebugLED object.
     * @note This method exists only because is necessary for creating timer inside class in freeRTOS.
     * 
     * This method is private.
     * @param xTimer FreeRTOS software timer.
     */
    static void startBlinkTimeoutHandle(TimerHandle_t xTimer);

    /**
     * @brief Creates and starts Blink Timeout Timer. If timer is already started, it will be restarted.
     * @note This method is private.
     * @param maxBlinkTime Time in milliseconds after which the timer will expire.
     */
    static void startBlinkTimeout(uint32_t maxBlinkTime);

    /**
     * @brief Deletes Blink Timeout Timer if exists.
     * @note This method is private.
     */
    static void deleteBlinkTimeout();

    static DebugLED* mspInstance;

    static TaskHandle_t msPairingBlinkHandle;
    static TaskHandle_t msResetBlinkHandle;
    static TimerHandle_t msBlinkTimeout;

    ul::Logger mLogger;
};