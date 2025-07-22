#include "communication/addressing/module_addressing.h"

#include <Arduino.h>

#include "smart_home_config.h"
#include "config/communication_config.h"
#include "communication/uint8_array_handlers.h"

namespace uah = uint8ArrayHandlers;

ModuleAddressing* ModuleAddressing::mspAddressing = nullptr;

// ============================ Public ============================

ModuleAddressing::ModuleAddressing(Communication *communication)
    : Addressing(communication) {
    mspAddressing = this;
    mIPAddress = 0; // 0 - NULL
    
    Serial.println("ModuleAddressing initialized");
    Serial.println(mIPAddress);
    for (int i = 0; i <6 ; i++){
        Serial.print(mMACAddress[i]);
        Serial.print(' ');
    }
    Serial.println();
}

ModuleAddressing::~ModuleAddressing() {
    deleteAddressingTask();
    deleteAddressingQueues();
    // deleteAddressingTimers();
}
// ================================================================

// ===================== Addressing Algorithm =====================

void ModuleAddressing::addressingTask(void* parameters) {
    for (;;) {
        Serial.println("ModuleAddressing::addressingTask");
        vTaskDelay(pdMS_TO_TICKS(1000));
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
void ModuleAddressing::deleteAddressingTask() {
    if (mAddressingTaskHandle != NULL) {
        vTaskDelete(mAddressingTaskHandle);
        mAddressingTaskHandle = NULL;
    }
}
// ================================================================

// // ============================ Timers ============================

// void ModuleAddressing::addressingTimersCallbacks(TimerHandle_t xTimer){
//     auto &ad = *mspAddressing;

//     if (xTimer == ad.mAddressingTimeoutTimer) {
//         // TODO implement
//         // TODO remove print
//         Serial.println("mAddressingTimeoutTimer");
//         // xTaskNotify(com.mCommunicationMainTaskHandle, messageTimeoutNotif, eSetValueWithOverwrite);
//     }
// }

// void ModuleAddressing::createAddressingTimers() {
//     if (mAddressingTimeoutTimer == NULL) {
//         mAddressingTimeoutTimer = xTimerCreate(
//             "Addressing Absolute Timeout",
//             // TODO change tmp
//             pdMS_TO_TICKS(5000),
//             // pdMS_TO_TICKS(ADDRESSING_ABSOLUTE_TIMEOUT),
//             pdFALSE,
//             NULL,
//             addressingTimersCallbacks
//         );
//     }
// }

// void ModuleAddressing::deleteAddressingTimers() {
//     if (mAddressingTimeoutTimer != NULL) {
//         xTimerDelete(mAddressingTimeoutTimer, portMAX_DELAY);
//         mAddressingTimeoutTimer = NULL;
//     }
// }
// // ================================================================