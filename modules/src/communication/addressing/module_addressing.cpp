#include "communication/addressing/module_addressing.h"

#include <Arduino.h>

#include "smart_home_config.h"
#include "config/communication_config.h"
#include "config/addressing_config.h"

#include "communication/uint8_array_handlers.h"
#include "communication/communication.h"

namespace uah = uint8ArrayHandlers;

ModuleAddressing* ModuleAddressing::mspAddressing = nullptr;

// ============================ Public ============================

ModuleAddressing::ModuleAddressing(Communication *communication)
    : Addressing(communication) {
    mspAddressing = this;
    mIPAddress = 0; // 0 - NULL
    
    Serial.println("ModuleAddressing initialized");
}

ModuleAddressing::~ModuleAddressing() {
    deleteAddressingTask();
    deleteAddressingQueues();
    deleteAddressingTimers();
}
// ================================================================

// ===================== Addressing Algorithm =====================

void ModuleAddressing::addressingTask(void* parameters) {
    auto &ad = *mspAddressing;

    xTimerStart(ad.mAddressingTimeoutTimer, portMAX_DELAY);
    for (;;) {
        ad.mpCommunication->sendMessage((uint8_t*)"AD 123");
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

void ModuleAddressing::createAddressingTask() {
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
// ================================================================

// ============================ Timers ============================

void ModuleAddressing::addressingTimersCallbacks(TimerHandle_t xTimer){
    auto &ad = *mspAddressing;

    if (xTimer == ad.mAddressingTimeoutTimer) {
        ad.abortAddressingWithAbortMessage();
    }
}

void ModuleAddressing::createAddressingTimers() {
    if (mAddressingTimeoutTimer == NULL) {
        mAddressingTimeoutTimer = xTimerCreate(
            "Addressing Absolute Timeout",
            pdMS_TO_TICKS(ADDRESSING_ABSOLUTE_TIMEOUT),
            pdFALSE,
            NULL,
            addressingTimersCallbacks
        );
    }
}
// ================================================================

// ============================ Other =============================

void ModuleAddressing::abortAddresing() {
    // TODO consider protecting data with mutex
    uah::prepareBuffor(mProtocolMACAddress, mMACAddress, 6, 6);
    mIPAddress = 0;

    mpCommunication->resetEncodeMessageTask();
    mpCommunication->sendInternalMessage((uint8_t*)"HC+DEFAULT");
    mpCommunication->stopAddresingAlgorithm();
}

// ================================================================