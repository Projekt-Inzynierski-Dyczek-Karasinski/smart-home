#pragma once

#include <Arduino.h>

// #include "smart_home_config.h"
// #include "config/communication_config.h"

#include "communication/addressing/addressing.h"

class Communication; 

class CentralUnitAddressing final : public Addressing{
public:
    explicit CentralUnitAddressing(Communication *communication);
    ~CentralUnitAddressing();

private:
    static void addressingTask(void* parameters);
    void createAddressingTask() override;

    static void addressingTimersCallbacks(TimerHandle_t xTimer);
    void createAddressingTimers() override;

    void abortAddresing() override;
    
    static CentralUnitAddressing *mspAddressing;
};