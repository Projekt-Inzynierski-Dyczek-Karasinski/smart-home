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
    mIPAddress = CENTRAL_UNIT_IP;

    mModulesAddressingDataMutex = xSemaphoreCreateMutex();
    // prepare array counting modules on rf channels and reserve place for central unit
    xSemaphoreTake(mModulesAddressingDataMutex, portMAX_DELAY);
    uah::prepareBuffor(mNumOFModulesOnRfChannel, MAX_NUM_OF_CHANNEL);
    mNumOFModulesOnRfChannel[0] = 1;
    xSemaphoreGive(mModulesAddressingDataMutex);

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

    enum ADDRESSING_STATES : uint8_t {
        START_ADDRESSING = 0,
        PINGING,
        SUMMARY
    };

    uint8_t receiveBuffor[MESSAGE_SIZE];
    uint8_t sendBuffor[MESSAGE_SIZE];

    xTimerStart(ad.mAddressingTimeoutTimer, portMAX_DELAY);

    uint8_t absoluteAttemptCounter = 0;
    for (;;) {
        if (absoluteAttemptCounter > ADDRESSING_ABSOLUTE_MAX_ATTEMPTS) {
            ad.abortAddressingWithAbortMessage();
            for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
        }
        absoluteAttemptCounter++;

        uint8_t addressingState = START_ADDRESSING;
        
        ad.mpCommunication->needRawMessage();
        xQueueReceive(ad.mAddressingQueue, receiveBuffor, portMAX_DELAY);
        
        uint8_t attemptCounter = 0;
        uint8_t moduleNewIP;
        uint8_t moduleNewRfChannel;
        AddressingData savedModuleData;
        for (;;) {
            if (attemptCounter > ADDRESSING_MAX_ATTEMPTS) {
                ad.sendRestartMessage();
                break;
            }

            // sending
            bool isRestarting = false;
            switch (addressingState) {
                case START_ADDRESSING:
                    // !remember in receiveBuffor is raw message!
                    // [0-5{mac}, 6{ip}, 7{messagesQuantity}, 8-13{message}, 14{checksum}, 15{\0}]
                    // TODO implement for other new connection messages
                    if (uah::areArraysEqual(&receiveBuffor[8], (uint8_t*)ADDRESSING_NC_REAL_MAC_RF_CHANNELS, ADDRESSING_API_LEN)) {
                        uint8_t moduleMAC[MAC_ADDRESS_LENGTH];
                        uah::prepareBuffor(moduleMAC, receiveBuffor, MAC_ADDRESS_LENGTH, MAC_ADDRESS_LENGTH);
                        moduleNewIP = ad.addModule(moduleMAC, true);
                        moduleNewRfChannel = ad.getModuleRfChannel(moduleNewIP);        

                        // send module information about module's new IP address and rf channel
                        uah::prepareBuffor(sendBuffor, (uint8_t*)ADDRESSING_NEW_IP_NEW_RF_CHANNEL, ADDRESSING_API_LEN, MESSAGE_SIZE);
                        sendBuffor[3] = moduleNewIP;
                        sendBuffor[5] = moduleNewRfChannel;
                        ad.mpCommunication->sendMessage(sendBuffor);

                        // wait for "repeat" message and give module time to switch rf channel
                        vTaskDelay(pdMS_TO_TICKS(ADDRESSING_MESSAGE_TIMEOUT/4));
                        
                        // change rf channel
                        uint8_t hc12Command[SETUP_COMMAND_SIZE];
                        uah::prepareBuffor(hc12Command, (uint8_t*)"HC+C000", 7, SETUP_COMMAND_SIZE);
                        hc12Command[6] = (moduleNewRfChannel % 10) + (uint8_t)'0';
                        hc12Command[5] = ((moduleNewRfChannel / 10) % 10) + (uint8_t)'0';
                        hc12Command[4] = (moduleNewRfChannel / 100) + (uint8_t)'0';
                        ad.mpCommunication->sendInternalMessage(hc12Command);

                        // do not wait for message, go to next stage
                        addressingState = PINGING;
                        continue;
                    } else {
                        Serial.println("ADDRESSING ERROR! In addressingTask() -> got bad new connection message.");
                        isRestarting = true;
                    }
                    break;

                case PINGING:
                    // send ping 
                    uah::prepareBuffor(sendBuffor, (uint8_t*)ADDRESSING_PING, ADDRESSING_API_LEN, ADDRESSING_API_LEN);
                    ad.mpCommunication->sendMessage(sendBuffor);
                    break;

                case SUMMARY:
                    ad.getModuleData(&savedModuleData, moduleNewIP);

                    uah::prepareBuffor(sendBuffor, (uint8_t*)ADDRESSING_SUMMARY, ADDRESSING_API_LEN, ADDRESSING_API_LEN);
                    sendBuffor[6]  = (uint8_t)'i';
                    sendBuffor[7]  = savedModuleData.ipAddress;
                    sendBuffor[8]  = (uint8_t)'c';
                    sendBuffor[9]  = savedModuleData.rfChannel;
                    sendBuffor[10] = (uint8_t)'r';
                    sendBuffor[11] = savedModuleData.isMACAddressReal;
                    sendBuffor[12] = (uint8_t)'m';
                    uah::prepareBuffor(&sendBuffor[13], savedModuleData.macAddress, MAC_ADDRESS_LENGTH, (MESSAGE_SIZE - 13));

                    ad.mpCommunication->sendMessage(sendBuffor);
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
                Serial.println("restarting after sending...");
                ad.sendRestartMessage();
                break;
            }

            // receiving
            if (xQueueReceive(ad.mAddressingQueue, receiveBuffor, pdMS_TO_TICKS(ADDRESSING_MESSAGE_TIMEOUT)) == pdTRUE) {
                // if received message to abort addressing
                if (uah::areArraysEqual(receiveBuffor, (uint8_t*)ADDRESSING_ABORT, ADDRESSING_API_LEN)) {
                    ad.abortAddresing();
                    for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
                } 
                // if received message to restart addressing
                else if (uah::areArraysEqual(receiveBuffor, (uint8_t*)ADDRESSING_RESTART, ADDRESSING_API_LEN)) {
                    ad.clearNewConnectionData();
                    break;
                } else {
                    bool isReceivedPropperMessage = false;
                    switch (addressingState) {
                        case START_ADDRESSING:
                            Serial.println("ADDRESSING ERROR! In addressingTask() -> got addressingState == START_ADDRESSING in receiving part of task, did you forget change?");
                            isRestarting = true;
                            break;

                        case PINGING:
                            if (uah::areArraysEqual(receiveBuffor, (uint8_t*)ADDRESSING_REPING, ADDRESSING_API_LEN)) {
                                isReceivedPropperMessage = true;
                                addressingState = SUMMARY;
                            }
                            break;

                        case SUMMARY:
                            if (uah::areArraysEqual(receiveBuffor, (uint8_t*)ADDRESSING_SUMMARY_OK, ADDRESSING_API_LEN)) {
                                isReceivedPropperMessage = true;
                                Serial.println("Addressing complete");
                                // TODO remove clearing data 
                                // uint8_t mTmpModuleIp = NULL_IP; // TODO remember to clear that after end of new connection
                                ad.abortAddresing();
                                for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
                            } else if (uah::areArraysEqual(receiveBuffor, (uint8_t*)ADDRESSING_SUMMARY_BAD, ADDRESSING_API_LEN)) {
                                isReceivedPropperMessage = true;
                                Serial.println("ADDRESSING ERROR! In addressingTask() -> module rejects summary");
                                isRestarting = true;
                            }
                            break;  
                        
                        default:
                            Serial.print("ADDRESSING ERROR! In addressingTask() -> got unknow addressingState: ");
                            Serial.println(addressingState);
                            isRestarting = true;
                            break;
                    }

                    if (isRestarting) {
                        // TODO remove print
                        Serial.println("restarting after receiving...");
                        ad.sendRestartMessage();
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

    // xSemaphoreTake(mAddressingDataMutex, portMAX_DELAY);
    // mIsAddressingWorking = true;
    // xSemaphoreGive(mAddressingDataMutex);
}

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

// =================== Modules Addressing Data ====================

void CentralUnitAddressing::printModulesAddressingData() {
    xSemaphoreTake(mModulesAddressingDataMutex, portMAX_DELAY);
    for (uint8_t i = 0; i < MAX_NUM_OF_MODULES; i++) {
        if (mModulesAddressingData[i].ipAddress != NULL_IP) {
            Serial.print("Module: ");
            Serial.print(i);
            Serial.print(", ip: ");
            Serial.print(mModulesAddressingData[i].ipAddress);
            Serial.print(", rf: ");
            Serial.print(mModulesAddressingData[i].rfChannel);
            Serial.print(", mac real: ");
            Serial.print(mModulesAddressingData[i].isMACAddressReal);
            Serial.print(", mac: ");
            uah::printArrayAsInt(mModulesAddressingData[i].macAddress, MAC_ADDRESS_LENGTH); 
        }
    }
    xSemaphoreGive(mModulesAddressingDataMutex);
}

void CentralUnitAddressing::printNumOFModulesOnRfChannels() {
    xSemaphoreTake(mModulesAddressingDataMutex, portMAX_DELAY);
    for (uint8_t i = 0; i < MAX_NUM_OF_CHANNEL; i++) {
        if (mNumOFModulesOnRfChannel[i] != 0) {
            Serial.print("channel: ");
            Serial.print((int)indexToRfChannel(i));
            Serial.print(" num: ");
            Serial.println(mNumOFModulesOnRfChannel[i]);
        }
    }
    xSemaphoreGive(mModulesAddressingDataMutex);
}

void CentralUnitAddressing::getModuleData(AddressingData *addressingData, const uint8_t ipAddress) {
    xSemaphoreTake(mModulesAddressingDataMutex, portMAX_DELAY);

    const uint8_t index = ipToIndex(ipAddress);
    addressingData->ipAddress = mModulesAddressingData[index].ipAddress;
    addressingData->isMACAddressReal = mModulesAddressingData[index].isMACAddressReal;
    addressingData->rfChannel = mModulesAddressingData[index].rfChannel;
    uah::prepareBuffor(addressingData->macAddress, mModulesAddressingData[index].macAddress, MAC_ADDRESS_LENGTH, MAC_ADDRESS_LENGTH);

    xSemaphoreGive(mModulesAddressingDataMutex);
}

uint8_t CentralUnitAddressing::getModuleRfChannel(const uint8_t ipAddress) {
    xSemaphoreTake(mModulesAddressingDataMutex, portMAX_DELAY);
    const uint8_t index = ipToIndex(ipAddress);
    const uint8_t rfChannel = mModulesAddressingData[index].rfChannel;
    xSemaphoreGive(mModulesAddressingDataMutex);

    return rfChannel;
}

uint8_t CentralUnitAddressing::addModule(const uint8_t *macAddress, const bool isMACAddressReal, uint8_t rfChannel) {
    xSemaphoreTake(mModulesAddressingDataMutex, portMAX_DELAY);
    // choose ip address
    uint8_t chosenIP = NULL_IP;
    for (uint8_t i = 0; i < MAX_NUM_OF_MODULES; i++) {
        if (mModulesAddressingData[i].ipAddress == NULL_IP) {
            // if choose chanel automatically
            if (rfChannel == 0) {
                uint8_t minNum = UINT8_MAX;
                uint8_t tmpChannel;
                for (uint8_t channelIndex = 0; channelIndex < MAX_NUM_OF_CHANNEL; channelIndex++) {
                    if (mNumOFModulesOnRfChannel[channelIndex] == 0) {
                        tmpChannel = channelIndex;
                        break;
                    } else if (mNumOFModulesOnRfChannel[channelIndex] < minNum) {
                        minNum = mNumOFModulesOnRfChannel[channelIndex];
                        tmpChannel = channelIndex;
                    }
                }

                rfChannel = indexToRfChannel(tmpChannel);
            } 
            mNumOFModulesOnRfChannel[rfChannelToIndex(rfChannel)]++;

            // save data about module
            chosenIP = indexToIP(i);
            mModulesAddressingData[i].ipAddress = chosenIP;
            uah::prepareBuffor(mModulesAddressingData[i].macAddress, macAddress, MAC_ADDRESS_LENGTH, MAC_ADDRESS_LENGTH);
            mModulesAddressingData[i].rfChannel = rfChannel;
            mModulesAddressingData[i].isMACAddressReal = isMACAddressReal;
            mTmpModuleIp = chosenIP;

            break;
        }
    }
    xSemaphoreGive(mModulesAddressingDataMutex);

    if (chosenIP == NULL_IP) {
        Serial.println("CHOOSING IP ERROR! In addModule() -> not found free IP address");
    }
    return chosenIP;
}

void CentralUnitAddressing::removeModule(const uint8_t ipAddress) {
    xSemaphoreTake(mModulesAddressingDataMutex, portMAX_DELAY);

    const uint8_t INDEX = ipToIndex(ipAddress);
    mNumOFModulesOnRfChannel[rfChannelToIndex(mModulesAddressingData[INDEX].rfChannel)]--;

    mModulesAddressingData[INDEX].isMACAddressReal = true;
    mModulesAddressingData[INDEX].ipAddress = NULL_IP;
    mModulesAddressingData[INDEX].rfChannel = DEFAULT_CHANNEL;
    uah::prepareBuffor(mModulesAddressingData[INDEX].macAddress, MAC_ADDRESS_LENGTH);
    mTmpModuleIp = NULL_IP;

    xSemaphoreGive(mModulesAddressingDataMutex);
}

uint8_t CentralUnitAddressing::getTmpModuleIp() {
    xSemaphoreTake(mModulesAddressingDataMutex, portMAX_DELAY);
    const uint8_t tmpModuleIp = mTmpModuleIp;
    xSemaphoreGive(mModulesAddressingDataMutex);

    return tmpModuleIp;
} 

// ================================================================

// ============================ Other =============================

void CentralUnitAddressing::clearNewConnectionData() {
    Serial.println("Clearing new connection data...");
    const uint8_t tmpModuleIp = getTmpModuleIp();
    if (tmpModuleIp != NULL_IP) {
        removeModule(tmpModuleIp);
    }

    mpCommunication->sendInternalMessage((uint8_t*)"HC+DEFAULT");
    vTaskDelay(pdMS_TO_TICKS(ADDRESSING_DELAY_BETWEEN_ATTEMPTS));
    xQueueReset(mAddressingQueue);
}

void CentralUnitAddressing::abortAddresing() {
    Serial.println("Aborting addressing...");
    clearNewConnectionData();
    
    mpCommunication->stopAddresingAlgorithm();
}
// ================================================================