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
    mIPAddress = NULL_IP; 
    
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

    enum ADDRESSING_STATES : uint8_t {
        START_ADDRESSING = 0,
        CHANGE_RH_CHANNEL,
        SUMMARY
    };
    uint8_t addressingState = START_ADDRESSING;

    uint8_t receiveBuffor[MESSAGE_SIZE];
    uint8_t lastReceivedMessageBuffor[MESSAGE_SIZE];
    uint8_t sendBuffor[MESSAGE_SIZE];

    uint8_t attemptCounter = 0;

    xTimerStart(ad.mAddressingTimeoutTimer, portMAX_DELAY);
    for (;;) {
        if (attemptCounter > ADDRESSING_MAX_ATTEMPTS) {
            ad.abortAddressingWithAbortMessage();
            for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
        }

        // sending
        switch (addressingState) {
        case START_ADDRESSING:
            #ifdef ESP32_BOARD
                #ifdef RF_CHANNELS
                    uah::prepareBuffor(sendBuffor, (uint8_t*)"ADncry", 6, MESSAGE_SIZE);
                #else 
                    uah::prepareBuffor(sendBuffor, (uint8_t*)"ADncrn", 6, MESSAGE_SIZE);
                #endif
            #else
                #error "Not implemented"
            #endif
            ad.mpCommunication->needRawMessage();
            ad.mpCommunication->sendMessage(sendBuffor);
            break;

        case CHANGE_RH_CHANNEL:
            Serial.println("implement CHANGE_RH_CHANNEL");
            ad.abortAddressingWithAbortMessage();
            for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
            break;
                
        case SUMMARY:
            Serial.println("implement SUMMARY");
            ad.abortAddressingWithAbortMessage();
            for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
            break;

        default:
            Serial.print("ADDRESSING ERROR! In addressingTask() -> got unknow addressingState: ");
            Serial.println(addressingState);
            break;
        }

        // receiving
        if (xQueueReceive(ad.mAddressingQueue, receiveBuffor, pdMS_TO_TICKS(ADDRESSING_MESSAGE_TIMEOUT)) == pdTRUE) {
            // if received message to abort addressing
            if (uah::areArraysEqual(receiveBuffor, (uint8_t*)"ADabor", 6)) {
                ad.abortAddresing();
            }
            // TODO change len (6) passed in areArraysEqual() ?
            // if message is same as last received message
            else if (uah::areArraysEqual(receiveBuffor, lastReceivedMessageBuffor, 6)) {
                attemptCounter++;
            } else {
                bool isReceivedPropperMessage = false;

                switch (addressingState) {
                case START_ADDRESSING:
                    // !remember in receiveBuffor is raw message!
                    // [0-5{mac}, 6{ip}, 7{messagesQuantity}, 8-13{message}, 14{checksum}, 15{\0}]
                    #ifdef RF_CHANNELS
                        // check is received propper message (ADi?c?), indexes are offset due to reading raw message
                        if (receiveBuffor[10] == (uint8_t)'i' && receiveBuffor[12] == (uint8_t)'c') {
                            isReceivedPropperMessage = true;
                            // TODO !BEFORE PULL REQUEST! check is working properly
                            ad.updateAddresingData(receiveBuffor, receiveBuffor[11], receiveBuffor[13]);
                            addressingState = CHANGE_RH_CHANNEL;
                            attemptCounter = 0;
                        }
                    #else 
                        // check is received propper message (ADi?)
                        if (receiveBuffor[10] == (uint8_t)'i' && receiveBuffor[12] == (uint8_t)BLANK_CHARACTER) {
                            isReceivedPropperMessage = true;
                            ad.updateAddresingData(receiveBuffor, receiveBuffor[11]);
                            addressingState = SUMMARY;
                            attemptCounter = 0;
                        }
                    #endif
                    break;

                case CHANGE_RH_CHANNEL:
                    Serial.println("implement CHANGE_RH_CHANNEL");
                    ad.abortAddressingWithAbortMessage();
                    for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
                    break;

                case SUMMARY:
                    Serial.println("implement SUMMARY");
                    ad.abortAddressingWithAbortMessage();
                    for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
                    break;

                default:
                    Serial.print("ADDRESSING ERROR! In addressingTask() -> got unknow addressingState: ");
                    Serial.println(addressingState);
                    break;
                }

                if (!isReceivedPropperMessage) {
                    attemptCounter++;
                }
            }
        } else {
            attemptCounter++;
        }
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

void ModuleAddressing::updateAddresingData(const uint8_t *newMAC, const uint8_t newIP) {
    // TODO consider protecting data with mutex
    uah::prepareBuffor(mProtocolMACAddress, newMAC, MAC_ADDRESS_LENGTH, MAC_ADDRESS_LENGTH);
    mIPAddress = newIP;
    mpCommunication->resetEncodeMessageTask();
}

#ifdef HC12_MODULE
void ModuleAddressing::updateAddresingData(const uint8_t *newMAC, const uint8_t newIP, const uint8_t newRfChannel) {
    // TODO consider protecting data with mutex
    uah::prepareBuffor(mProtocolMACAddress, newMAC, MAC_ADDRESS_LENGTH, MAC_ADDRESS_LENGTH);
    mIPAddress = newIP;
    mRfChannel = newRfChannel;

    mpCommunication->resetEncodeMessageTask();
}
#endif

void ModuleAddressing::abortAddresing() {
    #ifdef HC12_MODULE
    updateAddresingData(mMACAddress, NULL_IP, DEFAULT_CHANNEL);
    #else 
    updateAddresingData(mMACAddress, NULL_IP);
    #endif
    mpCommunication->sendInternalMessage((uint8_t*)"HC+DEFAULT");
    mpCommunication->stopAddresingAlgorithm();
}

// ================================================================