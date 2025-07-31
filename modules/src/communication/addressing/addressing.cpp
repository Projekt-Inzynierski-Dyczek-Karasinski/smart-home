#include "communication/addressing/addressing.h"

#include <Arduino.h>

#include "smart_home_config.h"
#include "config/communication_config.h"
#include "config/addressing_config.h"
#include "communication/uint8_array_handlers.h"
#include "communication/communication.h"

namespace uah = uint8ArrayHandlers;

// ============================ Public ============================

Addressing::Addressing(Communication *communication) 
    : mpCommunication(communication) {
    #ifdef ESP32_BOARD
        esp_read_mac(mMACAddress, ESP_MAC_WIFI_STA);
        esp_read_mac(mProtocolMACAddress, ESP_MAC_WIFI_STA);
    #else
        // TODO add function to get MAC address on different boards
        #error "MAC address not implemented!"
    #endif
    mAddressingDataMutex = xSemaphoreCreateMutex();
}

Addressing::~Addressing() = default;

void Addressing::getProtocolMACAddress(uint8_t macAddress[6]) {
    xSemaphoreTake(mAddressingDataMutex, portMAX_DELAY);
    uah::prepareBuffer(macAddress, mProtocolMACAddress, MAC_ADDRESS_LENGTH, MAC_ADDRESS_LENGTH);
    xSemaphoreGive(mAddressingDataMutex);
}

uint8_t Addressing::getIPAddress() {
    xSemaphoreTake(mAddressingDataMutex, portMAX_DELAY);
    const uint8_t ipAddress = mIPAddress;
    xSemaphoreGive(mAddressingDataMutex);

    return ipAddress;
}

void Addressing::startAddressing() {
    createAddressingTimer();
    createAddressingQueue();
    createAddressingTask();
}

void Addressing::stopAddressing() {
    deleteAddressingTask();
    deleteAddressingQueue();
    deleteAddressingTimer();
}

void Addressing::addMessage(const uint8_t message[MESSAGE_SIZE]) {
    if (mAddressingQueue != NULL) {
        xQueueSend(mAddressingQueue, message, portMAX_DELAY);
    } else {
        Serial.println("ADDRESSING ERROR! In addMessage() -> can't add message to queue, because queue doesn't exist");
    }
}
// ================================================================

// ============================ Queues ============================

void Addressing::createAddressingQueue() {
    if (mAddressingQueue == NULL) {
        mAddressingQueue = xQueueCreate(MESSAGE_QUEUE_LEN, sizeof(uint8_t[MESSAGE_SIZE]));
    }
}

void Addressing::deleteAddressingQueue() {
    if (mAddressingQueue != NULL) {
        vQueueDelete(mAddressingQueue);
        mAddressingQueue = NULL;
    }
}
// =================================================================

// ============================ Deletes ============================

void Addressing::deleteAddressingTask() {
    if (mAddressingTaskHandle != NULL) {
        vTaskDelete(mAddressingTaskHandle);
        mAddressingTaskHandle = NULL;
    }
}
void Addressing::deleteAddressingTimer() {
    if (mAddressingTimeoutTimer != NULL) {
        xTimerDelete(mAddressingTimeoutTimer, portMAX_DELAY);
        mAddressingTimeoutTimer = NULL;
    }
}
// ================================================================

// ============================ Other =============================

void Addressing::sendRestartMessage() {
    mpCommunication->sendMessage((uint8_t*)ADDRESSING_RESTART);
    clearNewConnectionData();
}

void Addressing::abortAddressingWithAbortMessage() {
    for (uint8_t i = 0; i < ADDRESSING_NUM_OF_ABORT_MESSAGES; i++) {
        mpCommunication->sendMessage((uint8_t*)ADDRESSING_ABORT);
        vTaskDelay(pdMS_TO_TICKS(ADDRESSING_DELAY_BETWEEN_ABORT_MESSAGES));
    }
    abortAddressing();
}
// ================================================================