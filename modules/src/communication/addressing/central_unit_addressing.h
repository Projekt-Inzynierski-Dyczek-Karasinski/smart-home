#pragma once

#include <Arduino.h>

#include "smart_home_config.h"
#include "config/communication_config.h"

#include "communication/addressing/addressing.h"

class Communication; 

// assuming that central unit is using hc12 to rf communication
#ifndef HC12_MODULE
    #error "Central unit must run with hc12 module"
#endif

class CentralUnitAddressing final : public Addressing{
public:
    explicit CentralUnitAddressing(Communication *communication);
    ~CentralUnitAddressing();

private:
    struct AddressingData {
        bool isMACAddressReal = true;
        uint8_t ipAddress = NULL_IP;
        uint8_t rfChannel = DEFAULT_CHANNEL;
        uint8_t macAddress[6] = {0, 0, 0, 0, 0, 0};
    };

    static void addressingTask(void* parameters);
    void createAddressingTask() override;

    static void addressingTimersCallbacks(TimerHandle_t xTimer);
    void createAddressingTimers() override;
    
    void printModulesAddressingData();
    void printNumOFModulesOnRfChannels();
    void getModuleData(AddressingData *addressingData, const uint8_t ipAddress);
    uint8_t getModuleRfChannel(const uint8_t ipAddress);
    uint8_t addModule(const uint8_t *macAddress, const bool isMACAddressReal, uint8_t rfChannel = 0);
    void removeModule(const uint8_t ipAddress);
    uint8_t getTmpModuleIp();

    void clearNewConnectionData() override;
    void abortAddresing() override;
    
    static CentralUnitAddressing *mspAddressing;

    AddressingData mModulesAddressingData[MAX_NUM_OF_MODULES]; 
    uint8_t mNumOFModulesOnRfChannel[MAX_NUM_OF_CHANNEL];
    uint8_t mTmpModuleIp = NULL_IP; // TODO remember to clear that after end of new connection

    SemaphoreHandle_t mModulesAddressingDataMutex = NULL;
};