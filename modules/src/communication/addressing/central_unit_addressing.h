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
    void deleteAddressingTask() override;

    static CentralUnitAddressing *mspAddressing;
};