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

    // TODO remove test
    printModulesAddressingData();
    printNumOFModulesOnRfChannels();
    Serial.println("----------");
    uint8_t mac1[] = {1,1,1,1,1,1};
    uint8_t mac2[] = {2,1,1,1,1,1};
    uint8_t mac3[] = {3,1,1,1,1,1};
    
    addModule(mac1, false);
    printModulesAddressingData();
    printNumOFModulesOnRfChannels();
    Serial.println("----------");

    addModule(mac2, false);
    printModulesAddressingData();
    printNumOFModulesOnRfChannels();
    Serial.println("----------");

    addModule(mac3, true, 3);
    printModulesAddressingData();
    printNumOFModulesOnRfChannels();
    Serial.println("----------");

    removeModule(2);
    printModulesAddressingData();
    printNumOFModulesOnRfChannels();
    Serial.println("----------");

    removeModule(3);
    printModulesAddressingData();
    printNumOFModulesOnRfChannels();
    Serial.println("----------");

    removeModule(4);
    printModulesAddressingData();
    printNumOFModulesOnRfChannels();

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
            uah::printArrayAsChar(buffor, MESSAGE_SIZE);
        }
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

void CentralUnitAddressing::getModuleData(AddressingData *addressingData, uint8_t ipAddress) {
    xSemaphoreTake(mModulesAddressingDataMutex, portMAX_DELAY);

    const uint8_t INDEX = ipToIndex(ipAddress);
    addressingData->ipAddress = mModulesAddressingData[INDEX].ipAddress;
    addressingData->isMACAddressReal = mModulesAddressingData[INDEX].isMACAddressReal;
    addressingData->rfChannel = mModulesAddressingData[INDEX].rfChannel;
    uah::prepareBuffor(addressingData->macAddress, mModulesAddressingData[INDEX].macAddress, MAC_ADDRESS_LENGTH, MAC_ADDRESS_LENGTH);

    xSemaphoreGive(mModulesAddressingDataMutex);
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

    xSemaphoreGive(mModulesAddressingDataMutex);
}

// ================================================================

// ============================ Other =============================

void CentralUnitAddressing::abortAddresing() {
    mpCommunication->sendInternalMessage((uint8_t*)"HC+DEFAULT");
    mpCommunication->stopAddresingAlgorithm();
}
// ================================================================