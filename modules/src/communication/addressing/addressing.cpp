#include "addressing.h"

#include <Arduino.h>
#include <memory>

#include "smart_home_config.h"
#include "config/communication_config.h"
#include "config/addressing_config.h"
#include "../../utils/uint8_array_handlers.h"
#include "communication/communication.h"

namespace uah = Utils::ArrayHandlers;


namespace Comms {
    // ============================ Public ============================
    Addressing::Addressing(Communication *communication, const std::shared_ptr<ul::Logger> &logger)
        : mpCommunication(communication) {
        #ifdef ESP32_BOARD
            esp_read_mac(mMACAddress, ESP_MAC_WIFI_STA);
            esp_read_mac(mProtocolMACAddress, ESP_MAC_WIFI_STA);
        #else
        // TODO add function to get MAC address on different boards
            #error "MAC address not implemented!"
        #endif
        mpLogger = logger;

        // TODO merge with main remove
        #ifdef COMMUNICATION_WITHOUT_SAVING_ADDRESSING
            const uint8_t tmpMAC[] = {1,1,1,1,1,1};
            uah::prepareBuffer(mProtocolMACAddress, tmpMAC, 6,6);
            mpLogger->warninga("Addressing TMP", "Protocol mac is forced: ", mProtocolMACAddress, 6, false);
        #endif

        mAddressingDataMutex = xSemaphoreCreateMutex();
    }

    Addressing::~Addressing() {
        deleteAddressingTask();
        deleteAddressingTimer();
        deleteAddressingQueue();

        vSemaphoreDelete(mAddressingDataMutex);
    }

    void Addressing::setProtocolIPAddress(uint8_t ip) {
        mpLogger->debug("Addressing Method", "Called setProtocolIPAddress()");
    }

    void Addressing::getProtocolMACAddress(uint8_t macAddress[MAC_ADDRESS_LENGTH]) const {
        xSemaphoreTake(mAddressingDataMutex, portMAX_DELAY);
        uah::prepareBuffer(macAddress, mProtocolMACAddress, MAC_ADDRESS_LENGTH, MAC_ADDRESS_LENGTH);
        xSemaphoreGive(mAddressingDataMutex);
    }

    void Addressing::startAddressing() {
        // TODO before merge with main remove commented code/rollback atomic
        // xSemaphoreTake(mAddressingDataMutex, portMAX_DELAY);
        // mIsAddressingInProgress = true;
        // xSemaphoreGive(mAddressingDataMutex);
        mIsAddressingInProgress.store(true);

        createAddressingTimer();
        createAddressingQueue();
        createAddressingTask();
    }

    void Addressing::stopAddressing() {
        deleteAddressingTask();
        deleteAddressingQueue();
        deleteAddressingTimer();

        // TODO before merge with main remove commented code/rollback atomic
        // xSemaphoreTake(mAddressingDataMutex, portMAX_DELAY);
        // mIsAddressingInProgress = false;
        // xSemaphoreGive(mAddressingDataMutex);
        mIsAddressingInProgress.store(false);
    }

    bool Addressing::getIsAddressingInProgress() const {
        // TODO before merge with main remove commented code/rollback atomic
        // xSemaphoreTake(mAddressingDataMutex, portMAX_DELAY);
        // const bool result = mIsAddressingInProgress;
        // xSemaphoreGive(mAddressingDataMutex);
        // return result;
        return mIsAddressingInProgress.load();
    }


    void Addressing::addMessage(const uint8_t message[MESSAGE_SIZE]) const {
        if (mAddressingQueue != nullptr) {
            xQueueSend(mAddressingQueue, message, portMAX_DELAY);
        } else {
            mpLogger->warning("Addressing FreeRTOS", "Can't add message to queue, because queue doesn't exist.");
        }
    }

    // ============================ Queues ============================
    void Addressing::createAddressingQueue() {
        if (mAddressingQueue == nullptr) {
            mAddressingQueue = xQueueCreate(MESSAGE_QUEUE_LEN, sizeof(uint8_t[MESSAGE_SIZE]));
        }
    }

    void Addressing::deleteAddressingQueue() {
        if (mAddressingQueue != nullptr) {
            vQueueDelete(mAddressingQueue);
            mAddressingQueue = nullptr;
        }
    }

    // ============================ Deletes ============================
    void Addressing::deleteAddressingTask() {
        if (mAddressingTaskHandle != nullptr) {
            vTaskDelete(mAddressingTaskHandle);
            mAddressingTaskHandle = nullptr;
        }
    }
    void Addressing::deleteAddressingTimer() {
        if (mAddressingTimeoutTimer != nullptr) {
            xTimerDelete(mAddressingTimeoutTimer, portMAX_DELAY);
            mAddressingTimeoutTimer = nullptr;
        }
    }

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

    bool Addressing::isAddressingFailed(const uint8_t *receiveBuffer) {
        // if received message to abort addressing
        if (uah::areArraysEqual(receiveBuffer, ADDRESSING_ABORT)) {
            abortAddressing();
            for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
            return true;
        }
        // if received message to restart addressing
        if (uah::areArraysEqual(receiveBuffer, ADDRESSING_RESTART)) {
            return true;
        }

        return false;
    }
}