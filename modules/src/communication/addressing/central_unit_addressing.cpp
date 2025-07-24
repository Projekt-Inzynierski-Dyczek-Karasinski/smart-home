#include "communication/addressing/central_unit_addressing.h"

#include <Arduino.h>

#include "smart_home_config.h"
#include "config/communication_config.h"
#include "config/addressing_config.h"

#include "communication/uint8_array_handlers.h"
#include "communication/communication.h"

namespace uah = uint8ArrayHandlers;

CentralUnitAddressing* CentralUnitAddressing::mspAddressing = nullptr;

// ============================ Public ============================

CentralUnitAddressing::CentralUnitAddressing(Communication *communication) 
    : Addressing(communication) {
    mspAddressing = this;
    mIPAddress = 1; // 1 - central unit's IP

    Serial.println("CentralUnitAddressing initialized");
}

CentralUnitAddressing::~CentralUnitAddressing() {
    deleteAddressingTask();
    deleteAddressingQueues();
    deleteAddressingTimers();
}

// ================================================================

// ===================== Addressing Algorithm =====================

void CentralUnitAddressing::addressingTask(void* parameters) {
    auto &ad = *mspAddressing;

    uint8_t buffor[MESSAGE_SIZE];
    xTimerStart(ad.mAddressingTimeoutTimer, portMAX_DELAY);
    for (;;) {
        if (xQueueReceive(ad.mAddressingQueue, buffor, 0) == pdTRUE) {
            uah::printArray(buffor, MESSAGE_SIZE);
        }
        // vTaskDelay(pdMS_TO_TICKS(1000));
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
// void CentralUnitAddressing::deleteAddressingTask() {
//     if (mAddressingTaskHandle != NULL) {
//         vTaskDelete(mAddressingTaskHandle);
//         mAddressingTaskHandle = NULL;
//     }
// }
// ================================================================


// ============================ Timers ============================

void CentralUnitAddressing::addressingTimersCallbacks(TimerHandle_t xTimer){
    auto &ad = *mspAddressing;

    if (xTimer == ad.mAddressingTimeoutTimer) {
        ad.abortAddressingWithAbortMessage();
    }
}

void CentralUnitAddressing::createAddressingTimers() {
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

void CentralUnitAddressing::abortAddresing() {
    mpCommunication->sendInternalMessage((uint8_t*)"HC+DEFAULT");
    mpCommunication->stopAddresingAlgorithm();
}
// ================================================================