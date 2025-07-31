#pragma once

#include <Arduino.h>

#include "config/communication_config.h"
#include "config/addressing_config.h"

class Communication; 

/**
 * @brief Abstract base class for ModuleAddressing and CentralUnitAddressing classes.
 *        Responsible for handling MAC and IP addresses.
 */
class Addressing {
public:
    /**
     * @brief Constructs an Addressing object.
     * @param communication Pointer to the Communication object.
     */
    explicit Addressing(Communication *communication);

    /**
     * @brief Virtual destructor for safe polymorphic deletion.
     */
    virtual ~Addressing();

    /**
     * @brief Copies the current protocol MAC address into the provided buffer.
     * @param macAddress Output buffer of length 6 for the MAC address.
     * @note Thread-safe.
     */
    void getProtocolMACAddress(uint8_t macAddress[MAC_ADDRESS_LENGTH]);

    /**
     * @brief Gets the currently assigned IP address for the module.
     * @return IP address.
     * @note Thread-safe.
     */
    uint8_t getIPAddress();

    /**
     * @brief Initializes addressing procedures and related FreeRTOS resources.
     */
    void startAddressing();

    /**
     * @brief Stops addressing procedures and cleans up FreeRTOS resources.
     */
    void stopAddressing();

    /**
     * @brief Adds a message to the addressing queue. Ensures that adding messages to queue is only available if queue exist.
     * @param message Array with message array to add to queue.
     */
    void addMessage(const uint8_t message[MESSAGE_SIZE]);

protected:
    /**
     * @brief Creates the FreeRTOS queue used for addressing messages.
     */
    void createAddressingQueue();

    /**
     * @brief Deletes the addressing queue and frees related resources.
     */
    void deleteAddressingQueue();

    /**
     * @brief Pure virtual function for creating the addressing task.
     * @warning Must be implemented by derived class.
     */
    virtual void createAddressingTask() = 0;

    /**
     * @brief Deletes the addressing task and frees related resources.
     */
    void deleteAddressingTask();

    /**
     * @brief Pure virtual function for creating the addressing timer.
     * @warning Must be implemented by derived class.
     */
    virtual void createAddressingTimer() = 0;

    /**
     * @brief Deletes the addressing timer and frees resources.
     */
    void deleteAddressingTimer();

    /**
     * @brief Pure virtual function to clear all data of a new connection.
     * @warning Must be implemented by derived class.
     */
    virtual void clearNewConnectionData() = 0;

    /**
     * @brief Sends a message to restart the addressing algorithm and calls clearNewConnectionData() method.
     */
    void sendRestartMessage();

    /**
     * @brief Pure virtual function to completely abort the addressing process.
     * @warning Must be implemented by derived class.      
     */
    virtual void abortAddressing() = 0;

    /**
     * @brief Sends a message to completely abort the addressing process and calls abortAddressing() method.
     */
    void abortAddressingWithAbortMessage();

    // TODO add led blinking indicating restart, success and failure of addressing 
    // void restartLedBlink();
    // void successLedBlink();
    // void abortLedBlink();

    Communication *mpCommunication; ///< Pointer to the Communication class instance (owner class).

    uint8_t mMACAddress[MAC_ADDRESS_LENGTH]; ///< Module's own MAC address.
    uint8_t mProtocolMACAddress[MAC_ADDRESS_LENGTH]; ///< Central unit's MAC address (or module's own MAC if unknown).
    uint8_t mIPAddress = NULL_IP; ///< Current assigned IP address (0 = NULL, 1 = central unit's IP).

    #ifdef ESP32_BOARD
        const bool m_IS_MAC_ADDRESS_REAL = true; ///< Indicates if MAC address is hardware-based (always true for ESP32).
    #else
        #error "MAC address not implemented!"
    #endif

    SemaphoreHandle_t mAddressingDataMutex = NULL; ///< Handle to mutex protecting access to addressing data.

    QueueHandle_t mAddressingQueue = NULL; ///< Handle to FreeRTOS queue for incoming addressing messages, queue length: 10x64 bytes (uint8_t).

    TaskHandle_t mAddressingTaskHandle = NULL; ///< Handle to the addressing FreeRTOS task.

    TimerHandle_t mAddressingTimeoutTimer = NULL; ///< Handle to the addressing timeout FreeRTOS software timer.
};