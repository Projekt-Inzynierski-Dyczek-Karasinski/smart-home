#pragma once

#include <Arduino.h>

#include "config/communication_config.h"
#include "config/addressing_config.h"

class Communication; 

class Addressing {
public:
    explicit Addressing(Communication *communication);
    virtual ~Addressing();

    // TODO consider changing this methods to voids that will set variable which pointer will be passed as param (to protect data with mutex)
    const uint8_t (&getProtocolMACAddress() const)[MAC_ADDRESS_LENGTH];
    const uint8_t getIPAddress();

    void startAddressing();
    void stopAddressing();

    void addMessage(const uint8_t MESSAGE[MESSAGE_SIZE]);

protected:
    void createAddressingQueues();
    void deleteAddressingQueues();

    virtual void createAddressingTask() = 0;
    void deleteAddressingTask();

    virtual void createAddressingTimers() = 0;
    void deleteAddressingTimers();

    virtual void abortAddresing() = 0;
    void abortAddressingWithAbortMessage();

    Communication *mpCommunication;

    uint8_t mMACAddress[MAC_ADDRESS_LENGTH]; // module's MAC address 
    uint8_t mProtocolMACAddress[MAC_ADDRESS_LENGTH]; // central unit's MAC address (if is known to the module. If isn't, it is same as mMACAddress)
    uint8_t mIPAddress = NULL_IP; // 0 is NULL, 1 is central unit's IP

    #ifdef ESP32_BOARD
        const bool M_IS_MAC_ADDRESS_REAL = true;
    #else
        #error "MAC address not implemented!"
    #endif
    
    QueueHandle_t mAddressingQueue = NULL;

    TaskHandle_t mAddressingTaskHandle = NULL;

    TimerHandle_t mAddressingTimeoutTimer = NULL;
};