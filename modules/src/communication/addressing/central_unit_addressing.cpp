#include "communication/addressing/central_unit_addressing.h"

#include <Arduino.h>

#include "smart_home_config.h"
#include "config/communication_config.h"
#include "communication/uint8_array_handlers.h"

namespace uah = uint8ArrayHandlers;

CentralUnitAddressing* CentralUnitAddressing::mspAddressing = nullptr;

// ============================ Public ============================

CentralUnitAddressing::CentralUnitAddressing(Communication *communication) 
    : Addressing(communication) {
    mspAddressing = this;
    mIPAddress = 1; // 1 - central unit's IP

    Serial.println("CentralUnitAddressing initialized");
    Serial.println(mIPAddress);
    for (int i = 0; i <6 ; i++){
        Serial.print(mMACAddress[i]);
        Serial.print(' ');
    }
    Serial.println();
}

CentralUnitAddressing::~CentralUnitAddressing() {
    deleteAddressingTask();
    deleteAddressingQueues();
    // deleteAddressingTimers();
}

// ================================================================

// ===================== Addressing Algorithm =====================

void CentralUnitAddressing::addressingTask(void* parameters) {
    for (;;) {
        Serial.println("CentralUnitAddressing::addressingTask");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void CentralUnitAddressing::createAddressingTask() {
    if (mAddressingTaskHandle == NULL) {
        xTaskCreate(
            addressingTask,
            "Addressing Task",
            2048,
            NULL,
            MEDIUM_TASK_PRIORITY,
            &mAddressingTaskHandle
        );
    } else {
        Serial.println("TASK CREATION ERROR! In createAddressingTask() -> Can't create addressing task, because task already exists");
    }
}
void CentralUnitAddressing::deleteAddressingTask() {
    if (mAddressingTaskHandle != NULL) {
        vTaskDelete(mAddressingTaskHandle);
        mAddressingTaskHandle = NULL;
    }
}
// ================================================================