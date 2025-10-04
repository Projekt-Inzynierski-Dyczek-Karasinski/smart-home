#include "module_addressing.h"

#include <nlohmann/json.hpp>

#include "utils/uint8_array_handlers.h"
#include "communication/communication.h"
#include "universal_module_system/data_manager.h"

namespace nl = nlohmann;
namespace uah = Utils::ArrayHandlers;
namespace ums = UniversalModuleSystem;

namespace Comms {
    ModuleAddressing* ModuleAddressing::mspAddressing = nullptr;

    // ============================ Public ============================
    ModuleAddressing::ModuleAddressing(Communication *communication, const std::shared_ptr<ul::Logger> &logger)
        : Addressing(communication, logger) {
        mspAddressing = this;
        mIPAddress = NULL_IP;

        const auto &dataManager = ums::DataManager::getInstance();

        const nl::json addressingData = dataManager.load(ADDRESSING_DATA_PATH);
        if (!addressingData.empty()) {
            mIPAddress = addressingData["IP"];
            // TODO consider creating function in uah
            for (uint8_t i = 0; i < MAC_ADDRESS_LENGTH; i++) {
                mProtocolMACAddress[i] = addressingData["MAC"][i].get<uint8_t>();
            }
            mRfChannel = addressingData["RFC"];
        }

        // TODO before merge with main remove
        #ifdef COMMUNICATION_WITHOUT_SAVING_ADDRESSING
            mIPAddress = 2;
        // TODO before merge with main remove commented code/rollback atomic
            // mRfChannel = 2;
            mRfChannel.store(2);
            mpCommunication->changeRFChannel(2);
            mpLogger->warningv("Addressing TMP", "IP is forced: ", mIPAddress);
        #endif

        mpLogger->info("ModuleAddressing Class", "ModuleAddressing initialized.");
    }

    uint8_t ModuleAddressing::getConnectionRFChannel() {
        return DEFAULT_CHANNEL;
    }

    uint8_t ModuleAddressing::getDefaultRFChannel() {
        uint8_t rfChannel = 0;
        #ifdef RF_CHANNELS
            // TODO before merge with main remove commented code/rollback atomic
            // xSemaphoreTake(mAddressingDataMutex, portMAX_DELAY);
            // rfChannel = mRfChannel;
            // xSemaphoreGive(mAddressingDataMutex);
            rfChannel = mRfChannel.load();
        #endif
        return rfChannel;
    }

    uint8_t ModuleAddressing::getIPAddress() {
        xSemaphoreTake(mAddressingDataMutex, portMAX_DELAY);
        const uint8_t ipAddress = mIPAddress;
        xSemaphoreGive(mAddressingDataMutex);
        return ipAddress;
    }

    bool ModuleAddressing::isMACValid(const uint8_t *mac) {
        bool result = false;
        xSemaphoreTake(mAddressingDataMutex, portMAX_DELAY);
        if (mIPAddress == NULL_IP || uah::areArraysEqual(mac, mProtocolMACAddress, MAC_ADDRESS_LENGTH)) {
            result = true;
        }
        xSemaphoreGive(mAddressingDataMutex);
        return result;
    }

    bool ModuleAddressing::isIpValid(const uint8_t ip) {
        bool result = false;
        // TODO before merge with main remove commented code/rollback atomic
        xSemaphoreTake(mAddressingDataMutex, portMAX_DELAY);
        if ((mIPAddress == NULL_IP && ip == CENTRAL_UNIT_IP) || ip == mIPAddress) {
            result = true;
        }
        xSemaphoreGive(mAddressingDataMutex);

        return result;
    }

    // ===================== Addressing Algorithm =====================
    void ModuleAddressing::addressingTask(void* parameters) {
        auto &ad = *mspAddressing;

        enum class ADDRESSING_STATES : uint8_t {
            START_ADDRESSING = 0,
            WAIT_FOR_PING,
            REPLY_PING,
            PROCESS_SUMMARY
        };

        uint8_t receiveBuffer[MESSAGE_SIZE];
        uint8_t sendBuffer[MESSAGE_SIZE];

        xTimerStart(ad.mAddressingTimeoutTimer, portMAX_DELAY);

        for (uint8_t absoluteAttemptCounter = 0; absoluteAttemptCounter < ADDRESSING_ABSOLUTE_MAX_ATTEMPTS; absoluteAttemptCounter++) {
            ADDRESSING_STATES addressingState = ADDRESSING_STATES::START_ADDRESSING;
            uint8_t attemptCounter = 0;

            // variables for data passed in PROCESS_SUMMARY
            uint8_t ipToCheck = NULL_IP;
            uint8_t rfChannelToCheck = 0;
            uint8_t macToCheck[6];
            bool isMacRealToCheck = false;

            while (attemptCounter < ADDRESSING_MAX_ATTEMPTS) {
                // sending
                bool isRestarting = false;
                switch (addressingState) {
                    case ADDRESSING_STATES::START_ADDRESSING:
                        #ifdef ESP32_BOARD
                            #ifdef RF_CHANNELS
                                uah::prepareBuffer(sendBuffer, ADDRESSING_NC_REAL_MAC_RF_CHANNELS, MESSAGE_SIZE);
                            #else
                                uah::prepareBuffer(sendBuffer, ADDRESSING_NC_REAL_MAC_NO_RF_CHANNELS, MESSAGE_SIZE);
                            #endif
                        #else
                            #error "Not implemented"
                        #endif
                        ad.mpCommunication->needRawMessage();
                        ad.mpCommunication->sendMessage(sendBuffer);
                        break;

                    case ADDRESSING_STATES::REPLY_PING:
                        // send reping
                        uah::prepareBuffer(sendBuffer, ADDRESSING_REPING, MESSAGE_SIZE);
                        ad.mpCommunication->sendMessage(sendBuffer);
                        addressingState = ADDRESSING_STATES::PROCESS_SUMMARY;
                        break;

                    case ADDRESSING_STATES::PROCESS_SUMMARY:
                        if (ipToCheck != NULL_IP) {
                            if (
                                ipToCheck == ad.getIPAddress() &&
                                rfChannelToCheck == ad.getDefaultRFChannel() &&
                                isMacRealToCheck == ad.m_IS_MAC_ADDRESS_REAL &&
                                uah::areArraysEqual(ad.mMACAddress, macToCheck, MAC_ADDRESS_LENGTH)
                            ) {
                                uah::prepareBuffer(sendBuffer, ADDRESSING_SUMMARY_OK, MESSAGE_SIZE);
                                ad.mpCommunication->sendMessage(sendBuffer);

                                ad.mpLogger->info("ModuleAddressing Main", "Addressing complete." );
                                // TODO before merge with main remove #ifndef directive and #else section
                                #ifndef COMMUNICATION_WITHOUT_SAVING_ADDRESSING
                                    nl::json addressingData;
                                    addressingData["IP"] = ad.getIPAddress();
                                    addressingData["RFC"] = ad.getDefaultRFChannel();
                                    addressingData["MAC"] = nl::json::array();
                                    uint8_t macToSave[MAC_ADDRESS_LENGTH];
                                    ad.getProtocolMACAddress(macToSave);
                                    // TODO consider making function in uah
                                    for (uint8_t i = 0; i< MAC_ADDRESS_LENGTH; i++) {
                                        addressingData["MAC"].push_back(macToSave[i]);
                                    }

                                    const auto &dataManager = ums::DataManager::getInstance();
                                    dataManager.save(ADDRESSING_DATA_PATH, addressingData);

                                    ad.mpCommunication->stopAddressingAlgorithm();
                                #else
                                    ad.abortAddressing();
                                #endif
                                for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
                            } else {
                                ad.mpLogger->warning("ModuleAddressing Main", "Central unit send bad data in summary." );
                                uah::prepareBuffer(sendBuffer, ADDRESSING_SUMMARY_BAD, MESSAGE_SIZE);
                                ad.mpCommunication->sendMessage(sendBuffer);
                                isRestarting = true;
                            }
                        }
                        break;

                    default:
                        break;
                }

                if (isRestarting) {
                    break;
                }

                // receiving
                if (xQueueReceive(ad.mAddressingQueue, receiveBuffer, pdMS_TO_TICKS(ADDRESSING_MESSAGE_TIMEOUT)) == pdTRUE) {
                    if (ad.isAddressingFailed(receiveBuffer)) {
                        break;
                    }

                    bool isReceivedPropperMessage = false;
                    switch (addressingState) {
                        case ADDRESSING_STATES::START_ADDRESSING:
                            // !remember in receiveBuffer is raw message!
                            // [0-5{mac}, 6{ip}, 7{messagesQuantity}, 8-13{message}, 14{checksum}, 15{\0}]
                            #ifdef RF_CHANNELS
                                // check is received propper message (ADi?c?), indexes are offset due to reading raw message
                                if (receiveBuffer[10] == (uint8_t)'i' && receiveBuffer[12] == (uint8_t)'c') {
                                    isReceivedPropperMessage = true;
                                    const uint8_t newRfChannel = receiveBuffer[13];

                                    ad.updateAddressingData(receiveBuffer, receiveBuffer[11], newRfChannel);
                                    ad.mpCommunication->changeRFChannel(newRfChannel);

                                    addressingState = ADDRESSING_STATES::WAIT_FOR_PING;
                                }
                            #else
                                // check is received propper message (AD?)
                                if (receiveBuffer[12] == (uint8_t)BLANK_CHARACTER) {
                                    isReceivedPropperMessage = true;
                                    ad.updateAddressingData(receiveBuffer, receiveBuffer[11]);
                                    addressingState = PROCESS_SUMMARY;
                                }
                            #endif
                            break;

                        case ADDRESSING_STATES::WAIT_FOR_PING:
                            if (uah::areArraysEqual(receiveBuffer, ADDRESSING_PING)) {
                                isReceivedPropperMessage = true;
                                ad.mpLogger->debug("ModuleAddressing Main", "Got ping.");
                                addressingState = ADDRESSING_STATES::REPLY_PING;
                            }
                            break;

                        case ADDRESSING_STATES::PROCESS_SUMMARY:
                            // if central unit is still sending ping
                            if (uah::areArraysEqual(receiveBuffer, ADDRESSING_PING)) {
                                addressingState = ADDRESSING_STATES::REPLY_PING;
                            } else if (
                                uah::areArraysEqual(receiveBuffer, ADDRESSING_SUMMARY) &&
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

    void ModuleAddressing::createAddressingTask() {
        if (mAddressingTaskHandle == nullptr) {
            xTaskCreate(
                addressingTask,
                "Addressing Task",
                MODULE_ADDRESSING_TASK_SIZE,
                nullptr,
                MEDIUM_TASK_PRIORITY,
                &mAddressingTaskHandle
            );
        } else {
            mpLogger->warning("ModuleAddressing FreeRTOS", "Can't create addressing task, because task already exists.");
        }
    }

    // ============================ Timers ============================
    void ModuleAddressing::addressingTimersCallbacks(TimerHandle_t xTimer){
        auto &ad = *mspAddressing;

        if (xTimer == ad.mAddressingTimeoutTimer) {
            ad.abortAddressingWithAbortMessage();
        }
    }

    void ModuleAddressing::createAddressingTimer() {
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
            // TODO before merge with main remove commented code/rollback atomic
            // mRfChannel = newRfChannel;
            mRfChannel.store(newRfChannel);
            xSemaphoreGive(mAddressingDataMutex);

            mpCommunication->resetEncodeMessageTask();
        }
    #endif

    void ModuleAddressing::clearNewConnectionData() {
        mpLogger->info("ModuleAddressing Main", "Clearing new connection data.");

        #ifdef RF_CHANNELS
            updateAddressingData(mMACAddress, NULL_IP, DEFAULT_CHANNEL);
        #else
            updateAddressingData(mMACAddress, NULL_IP);
        #endif

        mpCommunication->changeRFChannel(DEFAULT_CHANNEL);
        vTaskDelay(pdMS_TO_TICKS(ADDRESSING_DELAY_BETWEEN_ATTEMPTS));
        xQueueReset(mAddressingQueue);
    }

    void ModuleAddressing::abortAddressing() {
        mpLogger->warning("ModuleAddressing Main", "Aborting addressing.");
        clearNewConnectionData();
        mpCommunication->stopAddressingAlgorithm();
    }
}