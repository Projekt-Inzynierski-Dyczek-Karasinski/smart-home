#pragma once

#include <Arduino.h>

// #include "smart_home_config.h"
// #include "config/communication_config.h"

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
 
    static ModuleAddressing *mspAddressing;
    
};