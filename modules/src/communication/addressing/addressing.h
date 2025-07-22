#pragma once

#include <Arduino.h>

class Communication; 

class Addressing {
public:
    explicit Addressing(Communication *communication);
    virtual ~Addressing();

    // TODO consider changing this methods to voids that will set variable which pointer will be passed as param (to protect data with mutex)
    const uint8_t (&getProtocolMACAddress() const)[6];
    const uint8_t getIPAddress();

    void startAddressing();
    void stopAddressing();

protected:
    void createAddressingQueues();
    void deleteAddressingQueues();

    // static void addressingTask(void* parameters);
    virtual void createAddressingTask() = 0;
    virtual void deleteAddressingTask() = 0;

    // static void addressingTimersCallbacks(TimerHandle_t xTimer);
    // virtual void createAddressingTimers();
    // virtual void deleteAddressingTimers();


    Communication *mpCommunication;

    uint8_t mMACAddress[6]; // module's MAC address 
    uint8_t mProtocolMACAddress[6]; // central unit's MAC address (if is known to the module. If isn't, it is same as mMACAddress)
    uint8_t mIPAddress = 0; // 0 is NULL, 1 is central unit's IP

    #ifdef ESP32_BOARD
        const bool M_IS_MAC_ADDRESS_REAL = true;
    #else
        #error "MAC address not implemented!"
    #endif
    
    QueueHandle_t mAddressingQueue = NULL;

    TaskHandle_t mAddressingTaskHandle = NULL;

    TimerHandle_t mAddressingTimeoutTimer = NULL;
};