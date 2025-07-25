#pragma once

#include <Arduino.h>

#include "smart_home_config.h"
#include "config/addressing_config.h"

#include "communication/addressing/addressing.h"

class Communication; 

class ModuleAddressing final : public Addressing {
public:
    explicit ModuleAddressing(Communication *communication);
    ~ModuleAddressing();

private:
    static void addressingTask(void* parameters);
    void createAddressingTask();

    static void addressingTimersCallbacks(TimerHandle_t xTimer);
    void createAddressingTimers() override;

    void updateAddresingData(const uint8_t *newMAC, const uint8_t newIP);
    #ifdef HC12_MODULE
    void updateAddresingData(const uint8_t *newMAC, const uint8_t newIP, const uint8_t newRfChannel);
    #endif
    void abortAddresing() override;
 
    static ModuleAddressing *mspAddressing;

    #ifdef HC12_MODULE
    uint8_t mRfChannel = DEFAULT_CHANNEL;
    #endif
};