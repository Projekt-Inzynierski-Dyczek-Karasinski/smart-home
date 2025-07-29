#pragma once

#include <Arduino.h>

#include "config/communication_config.h"
#include "config/addressing_config.h"

class Communication; 

class Addressing {
public:
    explicit Addressing(Communication *communication);
    virtual ~Addressing();

    void getProtocolMACAddress(uint8_t macAddress[6]);
    uint8_t getIPAddress();
    bool getIsAddressingWorking();

    void startAddressing();
    void stopAddressing();

    void addMessage(const uint8_t message[MESSAGE_SIZE]);

protected:
    void createAddressingQueues();
    void deleteAddressingQueues();

    virtual void createAddressingTask() = 0;
    void deleteAddressingTask();

    virtual void createAddressingTimers() = 0;
    void deleteAddressingTimers();

    virtual void abortAddresing() = 0;
    void abortAddressingWithAbortMessage();

    bool mIsAddressingWorking = false;
    Communication *mpCommunication;

    uint8_t mMACAddress[MAC_ADDRESS_LENGTH]; // module's MAC address 
    uint8_t mProtocolMACAddress[MAC_ADDRESS_LENGTH]; // central unit's MAC address (if is known to the module. If isn't, it is same as mMACAddress)
    uint8_t mIPAddress = NULL_IP; // 0 is NULL, 1 is central unit's IP

    #ifdef ESP32_BOARD
        const bool m_IS_MAC_ADDRESS_REAL = true;
    #else
        #error "MAC address not implemented!"
    #endif
    SemaphoreHandle_t mAddressingDataMutex = NULL;

    QueueHandle_t mAddressingQueue = NULL;

    TaskHandle_t mAddressingTaskHandle = NULL;

    TimerHandle_t mAddressingTimeoutTimer = NULL;
};