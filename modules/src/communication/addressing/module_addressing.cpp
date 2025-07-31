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
    deleteAddressingQueue();
    deleteAddressingTimer();
}

#ifdef RF_CHANNELS
uint8_t ModuleAddressing::getRfChannel() {
    xSemaphoreTake(mAddressingDataMutex, portMAX_DELAY);
    uint8_t rfChannel = mRfChannel;
    xSemaphoreGive(mAddressingDataMutex);
    return rfChannel;
}
#endif
// ================================================================

// ===================== Addressing Algorithm =====================

void ModuleAddressing::addressingTask(void* parameters) {
    auto &ad = *mspAddressing;

    enum ADDRESSING_STATES : uint8_t {
        START_ADDRESSING = 0,
        WAIT_FOR_PING,
        REPING,
        SUMMARY
    };
    

    uint8_t receiveBuffer[MESSAGE_SIZE];
    uint8_t sendBuffer[MESSAGE_SIZE];

    xTimerStart(ad.mAddressingTimeoutTimer, portMAX_DELAY);

    uint8_t absoluteAttemptCounter = 0;
    for (;;) {
        if (absoluteAttemptCounter > ADDRESSING_ABSOLUTE_MAX_ATTEMPTS) {
            ad.abortAddressingWithAbortMessage();
            for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
        }
        absoluteAttemptCounter++;

        uint8_t addressingState = START_ADDRESSING;
        uint8_t attemptCounter = 0;

        // variables for data passed in SUMMARY
        uint8_t ipToCheck = NULL_IP;
        uint8_t rfChannelToCheck;
        uint8_t macToCheck[6];
        bool isMacRealToCheck;

        for (;;) {
            if (attemptCounter > ADDRESSING_MAX_ATTEMPTS) {
                ad.sendRestartMessage();
                break;
            }

            // sending
            bool isRestarting = false;
            switch (addressingState) {
                case START_ADDRESSING:
                    #ifdef ESP32_BOARD
                        #ifdef RF_CHANNELS
                            uah::prepareBuffer(sendBuffer, (uint8_t*)ADDRESSING_NC_REAL_MAC_RF_CHANNELS, ADDRESSING_API_LEN, MESSAGE_SIZE);
                        #else 
                            uah::prepareBuffer(sendBuffer, (uint8_t*)ADDRESSING_NC_REAL_MAC_NO_RF_CHANNELS, ADDRESSING_API_LEN, MESSAGE_SIZE);
                        #endif
                    #else
                        #error "Not implemented"
                    #endif
                    ad.mpCommunication->needRawMessage();
                    ad.mpCommunication->sendMessage(sendBuffer);
                    break;

                case WAIT_FOR_PING:
                    // do not do anything, wait for ping
                    break;

                case REPING:
                    // send reping 
                    uah::prepareBuffer(sendBuffer, (uint8_t*)ADDRESSING_REPING, ADDRESSING_API_LEN, ADDRESSING_API_LEN);
                    ad.mpCommunication->sendMessage(sendBuffer);
                    addressingState = SUMMARY;
                        
                case SUMMARY:
                    if (ipToCheck != NULL_IP) {
                        if (
                            ipToCheck == ad.getIPAddress() &&
                            rfChannelToCheck == ad.getRfChannel() &&
                            isMacRealToCheck == ad.m_IS_MAC_ADDRESS_REAL &&
                            uah::areArraysEqual(ad.mMACAddress, macToCheck, MAC_ADDRESS_LENGTH)
                        ) {
                            uah::prepareBuffer(sendBuffer, (uint8_t*)ADDRESSING_SUMMARY_OK, ADDRESSING_API_LEN, MESSAGE_SIZE);
                            ad.mpCommunication->sendMessage(sendBuffer);
                            // TODO add saving data in flash memory
                            Serial.println("Addressing complete");
                            // TODO remove clearing data 
                            ad.abortAddressing();
                            for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
                        } else {
                            Serial.println("ADDRESSING ERROR! In addressingTask() -> central unit send bad data in summary");
                            uah::prepareBuffer(sendBuffer, (uint8_t*)ADDRESSING_SUMMARY_BAD, ADDRESSING_API_LEN, MESSAGE_SIZE);
                            ad.mpCommunication->sendMessage(sendBuffer);
                            ad.clearNewConnectionData();
                            isRestarting = true;
                        }
                    }
                    break;

                default:
                    Serial.print("ADDRESSING ERROR! In addressingTask() -> got unknow addressingState: ");
                    Serial.println(addressingState);
                    ad.sendRestartMessage();
                    isRestarting = true;
                    break;
            }

            if (isRestarting) {
                // TODO remove print
                // Serial.println("restarting after sending...");
                break;
            }

            // receiving
            if (xQueueReceive(ad.mAddressingQueue, receiveBuffer, pdMS_TO_TICKS(ADDRESSING_MESSAGE_TIMEOUT)) == pdTRUE) {
                // if received message to abort addressing
                if (uah::areArraysEqual(receiveBuffer, (uint8_t*)ADDRESSING_ABORT, ADDRESSING_API_LEN)) {
                    ad.abortAddressing();
                } 
                // if received message to restart addressing
                else if (uah::areArraysEqual(receiveBuffer, (uint8_t*)ADDRESSING_RESTART, ADDRESSING_API_LEN)) {
                    ad.clearNewConnectionData();
                    break;
                } else {
                    bool isReceivedPropperMessage = false;

                    switch (addressingState) {
                    case START_ADDRESSING:
                        // !remember in receiveBuffer is raw message!
                        // [0-5{mac}, 6{ip}, 7{messagesQuantity}, 8-13{message}, 14{checksum}, 15{\0}]
                        #ifdef RF_CHANNELS
                            // check is received propper message (ADi?c?), indexes are offset due to reading raw message
                            if (receiveBuffer[10] == (uint8_t)'i' && receiveBuffer[12] == (uint8_t)'c') {
                                isReceivedPropperMessage = true;
                                uint8_t newRfChannel = receiveBuffer[13];

                                ad.updateAddressingData(receiveBuffer, receiveBuffer[11], newRfChannel);

                                // change rf channel
                                uint8_t hc12Command[SETUP_COMMAND_SIZE];
                                uah::prepareBuffer(hc12Command, (uint8_t*)"HC+C000", 7, SETUP_COMMAND_SIZE);
                                hc12Command[6] = (newRfChannel % 10) + (uint8_t)'0';
                                hc12Command[5] = ((newRfChannel / 10) % 10) + (uint8_t)'0';
                                hc12Command[4] = (newRfChannel / 100) + (uint8_t)'0';
                                ad.mpCommunication->sendInternalMessage(hc12Command);

                                addressingState = WAIT_FOR_PING;
                            }
                        #else 
                            // check is received propper message (ADi?)
                            if (receiveBuffer[10] == (uint8_t)'i' && receiveBuffer[12] == (uint8_t)BLANK_CHARACTER) {
                                isReceivedPropperMessage = true;
                                ad.updateAddressingData(receiveBuffer, receiveBuffer[11]);
                                addressingState = SUMMARY;
                            }
                        #endif
                        break;

                    case WAIT_FOR_PING:
                        if (uah::areArraysEqual(receiveBuffer, (uint8_t*)ADDRESSING_PING, ADDRESSING_API_LEN)) {
                            isReceivedPropperMessage = true;
                            addressingState = REPING;
                        }
                        break;

                    case REPING:
                        Serial.println("ADDRESSING ERROR! In addressingTask() -> addressingState == REPING in receiving part of task, did you forget change?");
                        ad.sendRestartMessage();
                        isRestarting = true;
                        break;

                    case SUMMARY:
                        // if central unit is still sending ping
                        if (uah::areArraysEqual(receiveBuffer, (uint8_t*)ADDRESSING_PING, ADDRESSING_API_LEN)) {
                            addressingState = REPING;
                        } else if (
                            uah::areArraysEqual(receiveBuffer, (uint8_t*)ADDRESSING_SUMMARY, ADDRESSING_API_LEN) &&
                            receiveBuffer[6]  == (uint8_t)'i' &&
                            receiveBuffer[8]  == (uint8_t)'c' &&
                            receiveBuffer[10] == (uint8_t)'r' &&
                            receiveBuffer[12] == (uint8_t)'m'
                        ) {
                            isReceivedPropperMessage = true;
                            ipToCheck = receiveBuffer[7];
                            rfChannelToCheck = receiveBuffer[9];
                            isMacRealToCheck = receiveBuffer[11];
                            uah::prepareBuffer(macToCheck, &receiveBuffer[13], MAC_ADDRESS_LENGTH, MAC_ADDRESS_LENGTH);
                        }
                        break;

                    default:
                        Serial.print("ADDRESSING ERROR! In addressingTask() -> got unknow addressingState: ");
                        Serial.println(addressingState);
                        ad.sendRestartMessage();
                        isRestarting = true;
                        break;
                    }

                    if (isRestarting) {
                        // TODO remove print
                        // Serial.println("restarting after receiving...");
                        break;
                    }

                    if (isReceivedPropperMessage) {
                        attemptCounter = 0;
                    } else {
                        attemptCounter++;
                    }
                }
            } else {
                attemptCounter++;
            }
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

void ModuleAddressing::createAddressingTimer() {
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

void ModuleAddressing::updateAddressingData(const uint8_t *newMAC, const uint8_t newIP) {
    xSemaphoreTake(mAddressingDataMutex, portMAX_DELAY);
    uah::prepareBuffer(mProtocolMACAddress, newMAC, MAC_ADDRESS_LENGTH, MAC_ADDRESS_LENGTH);
    mIPAddress = newIP;
    xSemaphoreGive(mAddressingDataMutex);
    mpCommunication->resetEncodeMessageTask();
}

#ifdef RF_CHANNELS
void ModuleAddressing::updateAddressingData(const uint8_t *newMAC, const uint8_t newIP, const uint8_t newRfChannel) {
    xSemaphoreTake(mAddressingDataMutex, portMAX_DELAY);
    uah::prepareBuffer(mProtocolMACAddress, newMAC, MAC_ADDRESS_LENGTH, MAC_ADDRESS_LENGTH);
    mIPAddress = newIP;
    mRfChannel = newRfChannel;
    xSemaphoreGive(mAddressingDataMutex);

    mpCommunication->resetEncodeMessageTask();
}
#endif

void ModuleAddressing::clearNewConnectionData() {
    Serial.println("Clearing new connection data...");

    #ifdef RF_CHANNELS
        updateAddressingData(mMACAddress, NULL_IP, DEFAULT_CHANNEL);
    #else 
        updateAddressingData(mMACAddress, NULL_IP);
    #endif

    #ifdef HC12_MODULE
        mpCommunication->sendInternalMessage((uint8_t*)"HC+DEFAULT");
    #endif
    vTaskDelay(pdMS_TO_TICKS(ADDRESSING_DELAY_BETWEEN_ATTEMPTS));
    xQueueReset(mAddressingQueue);
}


void ModuleAddressing::abortAddressing() {
    Serial.println("Aborting addressing...");

    clearNewConnectionData();

    mpCommunication->stopAddressingAlgorithm();
}

// ================================================================