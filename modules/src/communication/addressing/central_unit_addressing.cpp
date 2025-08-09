#include "communication/addressing/central_unit_addressing.h"

#include <Arduino.h>

#include "smart_home_config.h"
#include "config/communication_config.h"
#include "config/addressing_config.h"

#include "../../utils/uint8_array_handlers.h"
#include "communication/communication.h"

namespace uah = Utils::Uint8ArrayHandlers;

CentralUnitAddressing* CentralUnitAddressing::mspAddressing = nullptr;

// ============================ Public ============================

CentralUnitAddressing::CentralUnitAddressing(Communication *communication) 
    : Addressing(communication) {
    mspAddressing = this;
    mIPAddress = CENTRAL_UNIT_IP;

    mModulesAddressingDataMutex = xSemaphoreCreateMutex();

    // prepare array counting modules on rf channels and reserve place for central unit
    xSemaphoreTake(mModulesAddressingDataMutex, portMAX_DELAY);
    uah::clearBuffer(mNumOFModulesOnRfChannel, MAX_NUM_OF_CHANNEL);
    mNumOFModulesOnRfChannel[0] = 1;
    xSemaphoreGive(mModulesAddressingDataMutex);

    Serial.println("CentralUnitAddressing initialized");
}

CentralUnitAddressing::~CentralUnitAddressing() {
    deleteAddressingTask();
    deleteAddressingQueue();
    deleteAddressingTimer();
}

// ================================================================

// ===================== Addressing Algorithm =====================

void CentralUnitAddressing::prepareSummary(uint8_t *sendBuffer, const AddressingData *moduleData) {
    // TODO change API Calls to numeric values
    // prepare summary data (data without mac)
    const uint8_t summaryData[] = {
        (uint8_t)'i',
        moduleData->ipAddress,
        (uint8_t)'c',
        moduleData->rfChannel,
        (uint8_t)'r',
        moduleData->isMACAddressReal,
        (uint8_t)'m',
    };
    constexpr uint8_t summaryDataLen = 7;

    // prepare sendBuffer
    uah::prepareBuffer(sendBuffer, (uint8_t*)ADDRESSING_SUMMARY, ADDRESSING_API_LEN, ADDRESSING_API_LEN);
    uah::prepareBuffer(&sendBuffer[6], summaryData, summaryDataLen, summaryDataLen);
    uah::prepareBuffer(&sendBuffer[13], moduleData->macAddress, MAC_ADDRESS_LENGTH, (MESSAGE_SIZE - 13));     
}

void CentralUnitAddressing::addressingTask(void* parameters) {
    auto &ad = *mspAddressing;

    enum ADDRESSING_STATES : uint8_t {
        START_ADDRESSING = 0,
        PINGING,
        PROCESS_SUMMARY
    };

    uint8_t receiveBuffer[MESSAGE_SIZE];
    uint8_t sendBuffer[MESSAGE_SIZE];

    xTimerStart(ad.mAddressingTimeoutTimer, portMAX_DELAY);
    
    for (uint8_t absoluteAttemptCounter = 0; absoluteAttemptCounter < ADDRESSING_ABSOLUTE_MAX_ATTEMPTS; absoluteAttemptCounter++) {
        uint8_t addressingState = START_ADDRESSING;
        
        ad.mpCommunication->needRawMessage();
        xQueueReceive(ad.mAddressingQueue, receiveBuffer, portMAX_DELAY);
        
        uint8_t attemptCounter = 0;
        uint8_t moduleNewIP = NULL_IP;
        AddressingData savedModuleData;

        while (attemptCounter < ADDRESSING_MAX_ATTEMPTS) {
            // sending
            bool isRestarting = false;
            switch (addressingState) {
                case START_ADDRESSING:
                    // NOTE: receiveBuffer contains raw message!
                    // [0-5{mac}, 6{ip}, 7{messagesQuantity}, 8-13{message}, 14{checksum}, 15{\0}]
                    // TODO implement for other new connection messages
                    if (uah::areArraysEqual(&receiveBuffer[8], (uint8_t*)ADDRESSING_NC_REAL_MAC_RF_CHANNELS, ADDRESSING_API_LEN)) {
                        uint8_t moduleMAC[MAC_ADDRESS_LENGTH];
                        uah::prepareBuffer(moduleMAC, receiveBuffer, MAC_ADDRESS_LENGTH, MAC_ADDRESS_LENGTH);
                        moduleNewIP = ad.addModule(moduleMAC, true);
                        const uint8_t moduleNewRfChannel = ad.getModuleRfChannel(moduleNewIP);

                        // send module information about module's new IP address and rf channel
                        uah::prepareBuffer(sendBuffer, (uint8_t*)ADDRESSING_NEW_IP_NEW_RF_CHANNEL, ADDRESSING_API_LEN, MESSAGE_SIZE);
                        sendBuffer[3] = moduleNewIP;
                        sendBuffer[5] = moduleNewRfChannel;
                        ad.mpCommunication->sendMessage(sendBuffer);

                        // wait for "repeat" message and 
                        vTaskDelay(pdMS_TO_TICKS(ADDRESSING_MESSAGE_TIMEOUT/4));
                        // change rf channel
                        ad.changeRfChannel(moduleNewRfChannel);
                        // give module time to switch rf channel
                        vTaskDelay(pdMS_TO_TICKS(ADDRESSING_MESSAGE_TIMEOUT/4));
                        // go to next stage
                        addressingState = PINGING;
                        continue;
                    } else {
                        Serial.println("ADDRESSING ERROR! In addressingTask() -> got bad new connection message.");
                        isRestarting = true;
                    }
                    break;

                case PINGING:
                    // send ping 
                    uah::prepareBuffer(sendBuffer, (uint8_t*)ADDRESSING_PING, ADDRESSING_API_LEN, ADDRESSING_API_LEN);
                    ad.mpCommunication->sendMessage(sendBuffer);
                    // TODO remove print
                    // Serial.println("sending ping");
                    break;

                case PROCESS_SUMMARY:
                    ad.getModuleData(&savedModuleData, moduleNewIP);
                    ad.prepareSummary(sendBuffer, &savedModuleData);
                    ad.mpCommunication->sendMessage(sendBuffer);
                    break;            
                
                default:
                    Serial.print("ADDRESSING ERROR! In addressingTask() -> got unknow addressingState: ");
                    Serial.println(addressingState);
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
                if (ad.isAddressingFailed(receiveBuffer)) {
                    break;
                }

                bool isReceivedPropperMessage = false;
                switch (addressingState) {
                    case START_ADDRESSING:
                        Serial.println("ADDRESSING ERROR! In addressingTask() -> got addressingState == START_ADDRESSING in receiving part of task, did you forget change?");
                        isRestarting = true;
                        break;

                    case PINGING:
                        if (uah::areArraysEqual(receiveBuffer, (uint8_t*)ADDRESSING_REPING, ADDRESSING_API_LEN)) {
                            isReceivedPropperMessage = true;
                            addressingState = PROCESS_SUMMARY;
                        }
                        break;

                    case PROCESS_SUMMARY:
                        if (uah::areArraysEqual(receiveBuffer, (uint8_t*)ADDRESSING_SUMMARY_OK, ADDRESSING_API_LEN)) {
                            isReceivedPropperMessage = true;
                            Serial.println("Addressing complete");
                            // TODO remove clearing data 
                            // uint8_t mTmpModuleIp = NULL_IP; // TODO remember to clear that after end of new connection
                            ad.abortAddressing();
                            for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
                        } else if (uah::areArraysEqual(receiveBuffer, (uint8_t*)ADDRESSING_SUMMARY_BAD, ADDRESSING_API_LEN)) {
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
                    // Serial.println("restarting after receiving...");
                    ad.sendRestartMessage();
                    break;
                }

                if (isReceivedPropperMessage) {
                    attemptCounter = 0;
                } else {
                    attemptCounter++;
                }
            
            } else {
                attemptCounter++;
            }
        }

        ad.sendRestartMessage();
    }

    ad.abortAddressingWithAbortMessage();
    for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
}

void CentralUnitAddressing::createAddressingTask() {
    if (mAddressingTaskHandle == nullptr) {
        xTaskCreate(
            addressingTask,
            "Addressing Task",
            2048,
            nullptr,
            MEDIUM_TASK_PRIORITY,
            &mAddressingTaskHandle
        );
    } else {
        Serial.println("TASK CREATION ERROR! In createAddressingTask() -> Can't create addressing task, because task already exists");
    }
}

// ================================================================

// ============================ Timers ============================

void CentralUnitAddressing::addressingTimersCallbacks(TimerHandle_t xTimer) {
    auto &ad = *mspAddressing;

    if (xTimer == ad.mAddressingTimeoutTimer) {
        ad.abortAddressingWithAbortMessage();
    }
}

void CentralUnitAddressing::createAddressingTimer() {
    if (mAddressingTimeoutTimer == nullptr) {
        mAddressingTimeoutTimer = xTimerCreate(
            "Addressing Absolute Timeout",
            pdMS_TO_TICKS(ADDRESSING_ABSOLUTE_TIMEOUT),
            pdFALSE,
            nullptr,
            addressingTimersCallbacks
        );
    }
}
// ================================================================

// =================== Modules Addressing Data ====================

void CentralUnitAddressing::printModulesAddressingData() const {
    xSemaphoreTake(mModulesAddressingDataMutex, portMAX_DELAY);
    for (uint8_t i = 0; i < MAX_NUM_OF_MODULES; i++) {
        if (mModulesAddressingData[i].ipAddress != NULL_IP) {
            char buffer[48];
            sprintf(
                buffer, "Module: %d, ip: %d, rf: %d, mac real: %d, mac: ", i, 
                mModulesAddressingData[i].ipAddress, 
                mModulesAddressingData[i].rfChannel, 
                mModulesAddressingData[i].isMACAddressReal
            );
            Serial.print(buffer);
            uah::printArrayAsInt(mModulesAddressingData[i].macAddress, MAC_ADDRESS_LENGTH); 
        }
    }
    xSemaphoreGive(mModulesAddressingDataMutex);
}

void CentralUnitAddressing::printNumOFModulesOnRfChannels() const {
    xSemaphoreTake(mModulesAddressingDataMutex, portMAX_DELAY);
    for (uint8_t i = 0; i < MAX_NUM_OF_CHANNEL; i++) {
        if (mNumOFModulesOnRfChannel[i] != 0) {
            char buffer[20];
            sprintf(
                buffer, 
                "channel: %d num: %d", 
                (int)indexToRfChannel(i), 
                mNumOFModulesOnRfChannel[i]
            );
            Serial.println(buffer);
        }
    }
    xSemaphoreGive(mModulesAddressingDataMutex);
}

void CentralUnitAddressing::getModuleData(AddressingData *addressingData, const uint8_t ipAddress) const {
    xSemaphoreTake(mModulesAddressingDataMutex, portMAX_DELAY);

    const uint8_t index = ipToIndex(ipAddress);
    addressingData->ipAddress = mModulesAddressingData[index].ipAddress;
    addressingData->isMACAddressReal = mModulesAddressingData[index].isMACAddressReal;
    addressingData->rfChannel = mModulesAddressingData[index].rfChannel;
    uah::prepareBuffer(addressingData->macAddress, mModulesAddressingData[index].macAddress, MAC_ADDRESS_LENGTH, MAC_ADDRESS_LENGTH);

    xSemaphoreGive(mModulesAddressingDataMutex);
}

uint8_t CentralUnitAddressing::getModuleRfChannel(const uint8_t ipAddress) const {
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
            uah::prepareBuffer(mModulesAddressingData[i].macAddress, macAddress, MAC_ADDRESS_LENGTH, MAC_ADDRESS_LENGTH);
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

    const uint8_t index = ipToIndex(ipAddress);
    mNumOFModulesOnRfChannel[rfChannelToIndex(mModulesAddressingData[index].rfChannel)]--;

    mModulesAddressingData[index].isMACAddressReal = true;
    mModulesAddressingData[index].ipAddress = NULL_IP;
    mModulesAddressingData[index].rfChannel = DEFAULT_CHANNEL;
    uah::clearBuffer(mModulesAddressingData[index].macAddress, MAC_ADDRESS_LENGTH);
    mTmpModuleIp = NULL_IP;

    xSemaphoreGive(mModulesAddressingDataMutex);
}

uint8_t CentralUnitAddressing::getTmpModuleIp() const {
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

void CentralUnitAddressing::abortAddressing() {
    Serial.println("Aborting addressing...");
    clearNewConnectionData();
    
    mpCommunication->stopAddressingAlgorithm();
}
// ================================================================