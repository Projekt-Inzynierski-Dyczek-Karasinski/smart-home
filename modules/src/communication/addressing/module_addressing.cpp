#include "communication/addressing/module_addressing.h"

#include <Arduino.h>

#include "smart_home_config.h"
#include "config/communication_config.h"
#include "communication/uint8_array_handlers.h"

namespace uah = uint8ArrayHandlers;

ModuleAddressing* ModuleAddressing::mspAddressing = nullptr;

// ============================ Public ============================

ModuleAddressing::ModuleAddressing(Communication *communication) {
    mpCommunication = communication;
    mspAddressing = this;
    #ifdef ESP32_BOARD
        esp_read_mac(mMACAddress, ESP_MAC_WIFI_STA);
        esp_read_mac(mProtocolMACAddress, ESP_MAC_WIFI_STA);
    #else
        // TODO add function to get MAC address on different boards
        #error "MAC address not implemented!"
    #endif

    Serial.println("ModuleAddressing initialized");
}

ModuleAddressing::~ModuleAddressing() {
    deleteAddressingTask();
    deleteAddressingQueues();
}

const uint8_t (&ModuleAddressing::getProtocolMACAddress() const)[6] {
    return mProtocolMACAddress;
}

const uint8_t ModuleAddressing::getIPAddress() {
    return mIPAddress;
}

void ModuleAddressing::startAddressing() {
    createAddressingQueues();
    createAddressingTask();
}

void ModuleAddressing::stopAddressing() {
    deleteAddressingTask();
    deleteAddressingQueues();
}
// ================================================================


// ============================ Queues ============================

void ModuleAddressing::createAddressingQueues() {
    if (mAddressingQueue == NULL) {
        mAddressingQueue = xQueueCreate(MESSAGE_QUEUE_LEN, sizeof(uint8_t[MESSAGE_SIZE]));
    }
}

void ModuleAddressing::deleteAddressingQueues() {
    if (mAddressingQueue != NULL) {
        vQueueDelete(mAddressingQueue);
        mAddressingQueue = NULL;
    }
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