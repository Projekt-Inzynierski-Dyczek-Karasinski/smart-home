#ifndef DEBUG_LED_H
#define DEBUG_LED_H

#include <Arduino.h>

class DebugLED {
private:
    static TaskHandle_t msPairingBlinkHandle;
    static TaskHandle_t msResetBlinkHandle;
    static TimerHandle_t msBlinkTimeout;

    /**
     * @brief FreeRTOS task that blinks LED signalizing pairing process.
     */
    static void pairingBlink();

    /**
     * @brief Method that makes static cast of DebugLED object.
     * @note This method exists only because is necessary for creating task inside class in freeRTOS.
     * @param void* freeRTOS task parameters.
     */
    static void createPairingBlinkTaskHandle(void *parameters);

    /**
     * @brief FreeRTOS task that blinks LED signalizing reset process.
     */
    static void resetBlink();

    /**
     * @brief Method that makes static cast of DebugLED object.
     * @note This method exists only because is necessary for creating task inside class in freeRTOS.
     * @param void* freeRTOS task parameters.
     */
    static void createResetBlinkTaskHandle(void *parameters);

    /**
     * @brief Method that is called when the Blink Timeout Timer expires. Deletes Timer and Pairing Blink and Reset Blink tasks if exists.
     */
    static void blinkTimeoutCallback();

    /**
     * @brief Method that makes static cast of DebugLED object.
     * @note This method exists only because is necessary for creating timer inside class in freeRTOS.
     * @param TimerHandle_t freeRTOS software timer.
     */
    static void startBlinkTimeoutHandle(TimerHandle_t xTimer);

    /**
     * @brief Creates and starts Blink Timeout Timer. If timer is already started, it will be restarted.
     * @param uint32_t Time in milliseconds after which the timer will expire.
     */
    static void startBlinkTimeout(uint32_t maxBlinkTime);

    /**
     * @brief Deletes Blink Timeout Timer if exists.
     */
    static void deleteBlinkTimeout();
    
public:
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
     * @note This method is public.
     */
    static void createPairingBlinkTask();

    /**
     * @brief Deletes Pairing Blink Task and deletes Blink Timeout Timer if exists.
     * @note This method is public.
     */
    static void deletePairingBlinkTask();
    
    /**
     * @brief Creates Reset Blink Task and starts Blink Timeout Timer. If the task is already exist, only starts timer.
     * @warning If Pairing Blink Task exist, it will be deleted before creating Reset, only one of them is allowed at the same time.
     * @note This method is public.
     */
    static void createResetBlinkTask();

    /**
     * @brief Deletes Pairing Blink Task and deletes Blink Timeout Timer if exists.
     * @note This method is public.
     */
    static void deleteResetBlinkTask();
    
    /**
     * @brief Constructor of DebugLED class. Sets LED_PIN to OUTPUT and its state to LOW.
     */
    DebugLED();

    /**
     * @brief Destructor of DebugLED class. Deletes all class's tasks and timers.
     * @warning Destructor of this class exists only for programming principles. This class SHOULD BE NEVER deleted.
     */
    ~DebugLED();
};

#endif