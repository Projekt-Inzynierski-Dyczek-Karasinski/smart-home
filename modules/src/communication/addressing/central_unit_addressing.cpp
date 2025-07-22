#include "communication/addressing/central_unit_addressing.h"

#include <Arduino.h>

#include "smart_home_config.h"
#include "config/communication_config.h"
#include "communication/uint8_array_handlers.h"

namespace uah = uint8ArrayHandlers;

CentralUnitAddressing* CentralUnitAddressing::mspAddressing = nullptr;

// ============================ Public ============================

CentralUnitAddressing::CentralUnitAddressing(Communication *communication) {
    mpCommunication = communication;
    mspAddressing = this;
    #ifdef ESP32_BOARD
        esp_read_mac(mProtocolMACAddress, ESP_MAC_WIFI_STA);
    #else
        // TODO add function to get MAC address on different boards
        #error "MAC address not implemented!"
    #endif

    Serial.println("CentralUnitAddressing initialized");
}

CentralUnitAddressing::~CentralUnitAddressing() {
    deleteAddressingTask();
    deleteAddressingQueues();
}

const uint8_t (&CentralUnitAddressing::getProtocolMACAddress() const)[6] {
    return mProtocolMACAddress;
}

const uint8_t CentralUnitAddressing::getIPAddress() {
    return mIPAddress;
}

void CentralUnitAddressing::startAddressing() {
    createAddressingQueues();
    createAddressingTask();
}

void CentralUnitAddressing::stopAddressing() {
    deleteAddressingTask();
    deleteAddressingQueues();
}
// ================================================================


// ============================ Queues ============================

void CentralUnitAddressing::createAddressingQueues() {
    if (mAddressingQueue == NULL) {
        mAddressingQueue = xQueueCreate(MESSAGE_QUEUE_LEN, sizeof(uint8_t[MESSAGE_SIZE]));
    }
}

void CentralUnitAddressing::deleteAddressingQueues() {
    if (mAddressingQueue != NULL) {
        vQueueDelete(mAddressingQueue);
        mAddressingQueue = NULL;
    }
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