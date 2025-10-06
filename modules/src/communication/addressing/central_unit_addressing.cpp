#include "central_unit_addressing.h"

#include "utils/uint8_array_handlers.h"
#include "communication/communication.h"
#include "universal_module_system/data_manager.h"

namespace uah = Utils::ArrayHandlers;
namespace ums = UniversalModuleSystem;
namespace nl = nlohmann;


namespace Comms {
    CentralUnitAddressing* CentralUnitAddressing::mspAddressing = nullptr;

    // ============================ Public ============================
    CentralUnitAddressing::CentralUnitAddressing(Communication *communication, const std::shared_ptr<ul::Logger> &logger)
        : Addressing(communication, logger) {
        mspAddressing = this;
        mIPAddress = CENTRAL_UNIT_IP;

        mModulesAddressingDataMutex = xSemaphoreCreateMutex();

        // prepare array counting modules on rf channels and reserve place for central unit
        xSemaphoreTake(mModulesAddressingDataMutex, portMAX_DELAY);
        uah::clearBuffer(mNumOfModulesOnRfChannel, MAX_CHANNEL);
        mNumOfModulesOnRfChannel[0] = 1;
        xSemaphoreGive(mModulesAddressingDataMutex);

        // TODO before merge with main remove
        #ifdef COMMUNICATION_WITHOUT_SAVING_ADDRESSING
            uint8_t tmpMAC[] = {1,1,1,1,1,1};
            addModule(tmpMAC, true, 2);
            mpLogger->warning("CentralUnitAddressing TMP", "Hardcoded module");
        #endif

        loadModulesAddressingData();
        mpLogger->info("CentralUnitAddressing Class", "CentralUnitAddressing initialized.");
    }

    CentralUnitAddressing::~CentralUnitAddressing() {
        vSemaphoreDelete(mModulesAddressingDataMutex);
    }

    uint8_t CentralUnitAddressing::getConnectionRFChannel() {
        uint8_t rfChannel = DEFAULT_CHANNEL;
        xSemaphoreTake(mModulesAddressingDataMutex, portMAX_DELAY);
        if (mTmpModuleIp != NULL_IP) {
            const ModuleAddressingData moduleData = mModulesAddressingData[ipToIndex(mTmpModuleIp)];
            rfChannel = moduleData.rfChannel;
        } else {
            mpLogger->warning("CentralUnitAddressing Method", "No module selected.");
        }
        xSemaphoreGive(mModulesAddressingDataMutex);
        return rfChannel;
    }

    uint8_t CentralUnitAddressing::getDefaultRFChannel() {
        return DEFAULT_CHANNEL;
    }

    uint8_t CentralUnitAddressing::getIPAddress() {
        uint8_t result;
        xSemaphoreTake(mModulesAddressingDataMutex, portMAX_DELAY);
        if (mTmpModuleIp == NULL_IP || mIsStartOfAddressing) {
            result = CENTRAL_UNIT_IP;
        } else {
            result = mTmpModuleIp;
        }
        xSemaphoreGive(mModulesAddressingDataMutex);
        return result;
    }

    void CentralUnitAddressing::setProtocolIPAddress(const uint8_t ip) {
        if (ip == NULL_IP) {
            setTmpModuleIp(NULL_IP);
            mpLogger->info("CentralUnitAddressing Method", "Cleared protocol IP");
            return;
        }
        ModuleAddressingData moduleData;
        getModuleData(&moduleData, ip);
        if (moduleData.ipAddress != NULL_IP) {
            setTmpModuleIp(ip);
            mpLogger->infov("CentralUnitAddressing Method", "Changed protocol IP to: ", ip);
        } else {
            setTmpModuleIp(NULL_IP);
            mpLogger->errorv("CentralUnitAddressing Method", "Not found module of IP address: ", ip);
        }
    }

    bool CentralUnitAddressing::isMACValid(const uint8_t *mac) {
        bool result = false;
        if (getIsStartOfAddressing()) {
            result = true;
        } else {
            xSemaphoreTake(mAddressingDataMutex, portMAX_DELAY);
            result = uah::areArraysEqual(mac, mProtocolMACAddress, MAC_ADDRESS_LENGTH);
            xSemaphoreGive(mAddressingDataMutex);
        }
        return result;
    }

    bool CentralUnitAddressing::isIpValid(const uint8_t ip) {
        bool result = true;

        /* IP is not valid if:
        - It is the start of the addressing process (i.e., waiting for the first message from the module) and received IP is other than NULL_IP; OR
        - It is not the start of the addressing process and either CENTRAL_UNIT_IP was received (which is forbidden for modules) or NULL_IP was received (which is always forbidden except at the start of the addressing process). */
        if ((getIsStartOfAddressing() && ip != NULL_IP) ||
            (!getIsStartOfAddressing() && (ip == CENTRAL_UNIT_IP || ip == NULL_IP))) {
            result = false;
        }
        return result;
    }

    // ===================== Addressing Algorithm =====================
    void CentralUnitAddressing::prepareSummary(uint8_t *sendBuffer, const ModuleAddressingData *moduleData) {
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
        uah::prepareBuffer(sendBuffer, (uint8_t*)ADDRESSING_SUMMARY, SPECIAL_MESSAGE_LEN, SPECIAL_MESSAGE_LEN);
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

            ad.setIsStartOfAddressing(true);
            ad.mpCommunication->needRawMessage();
            xQueueReceive(ad.mAddressingQueue, receiveBuffer, portMAX_DELAY);

            uint8_t attemptCounter = 0;
            uint8_t moduleNewIP = NULL_IP;
            ModuleAddressingData savedModuleData;

            while (attemptCounter < ADDRESSING_MAX_ATTEMPTS) {
                // sending
                bool isRestarting = false;
                switch (addressingState) {
                    case START_ADDRESSING:
                        // NOTE: receiveBuffer contains raw message!
                        // [0-5{mac}, 6{ip}, 7{messagesQuantity}, 8-13{message}, 14{checksum}, 15{\0}]
                        // TODO implement for other new connection messages
                        if (uah::areArraysEqual(&receiveBuffer[8], (uint8_t*)ADDRESSING_NC_REAL_MAC_RF_CHANNELS, SPECIAL_MESSAGE_LEN)) {
                            uint8_t moduleMAC[MAC_ADDRESS_LENGTH];
                            uah::prepareBuffer(moduleMAC, receiveBuffer, MAC_ADDRESS_LENGTH, MAC_ADDRESS_LENGTH);
                            moduleNewIP = ad.addModule(moduleMAC, true);
                            const uint8_t moduleNewRfChannel = ad.getModuleRfChannel(moduleNewIP);

                            // send module information about module's new IP address and rf channel
                            uah::prepareBuffer(sendBuffer, ADDRESSING_NEW_IP_NEW_RF_CHANNEL, MESSAGE_SIZE);
                            sendBuffer[3] = moduleNewIP;
                            sendBuffer[5] = moduleNewRfChannel;
                            ad.mpCommunication->sendMessage(sendBuffer);

                            // wait for "repeat" message and
                            vTaskDelay(pdMS_TO_TICKS(ADDRESSING_MESSAGE_TIMEOUT/4));
                            // change rf channel
                            ad.mpCommunication->changeRFChannel(moduleNewRfChannel);
                            // give module time to switch rf channel
                            vTaskDelay(pdMS_TO_TICKS(ADDRESSING_MESSAGE_TIMEOUT/4));
                            // go to next stage
                            addressingState = PINGING;
                            continue;
                        } else {
                            ad.mpLogger->warning("CentralUnitAddressing Main", "Got bad new connection message.");
                            isRestarting = true;
                        }
                        break;

                    case PINGING:
                        // send ping
                        ad.setIsStartOfAddressing(false);
                        uah::prepareBuffer(sendBuffer, ADDRESSING_PING, MESSAGE_SIZE);
                        ad.mpCommunication->sendMessage(sendBuffer);
                        ad.mpLogger->debug("CentralUnitAddressing Main", "Sending ping.");
                        break;

                    case PROCESS_SUMMARY:
                        ad.getModuleData(&savedModuleData, moduleNewIP);
                        ad.prepareSummary(sendBuffer, &savedModuleData);
                        ad.mpCommunication->sendMessage(sendBuffer);
                        break;

                    default:
                        ad.mpLogger->errorv("CentralUnitAddressing Main", "Got unknow addressingState: ", addressingState);
                        isRestarting = true;
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
                        case START_ADDRESSING:
                            ad.mpLogger->error("CentralUnitAddressing Main", "Got addressingState == START_ADDRESSING in receiving part of task, did you forget change?");
                            isRestarting = true;
                            break;

                        case PINGING:
                            if (uah::areArraysEqual(receiveBuffer, ADDRESSING_REPING)) {
                                isReceivedPropperMessage = true;
                                addressingState = PROCESS_SUMMARY;
                            }
                            break;

                        case PROCESS_SUMMARY:
                            if (uah::areArraysEqual(receiveBuffer, ADDRESSING_SUMMARY_OK)) {
                                isReceivedPropperMessage = true;
                                ad.mpLogger->info("CentralUnitAddressing Main", "Addressing complete." );
                                // TODO before merge with main remove #ifndef directive and #else section
                                #ifndef COMMUNICATION_WITHOUT_SAVING_ADDRESSING
                                    ad.saveModulesAddressingData();
                                    ad.setTmpModuleIp(NULL_IP);
                                    ad.mpCommunication->stopAddressingAlgorithm();
                                    ad.mpCommunication->changeRFChannel(DEFAULT_CHANNEL);
                                #else
                                    // TODO remove clearing data
                                    ad.abortAddressing();
                                #endif
                                for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
                            } else if (uah::areArraysEqual(receiveBuffer, ADDRESSING_SUMMARY_BAD)) {
                                isReceivedPropperMessage = true;
                                ad.mpLogger->warning("CentralUnitAddressing Main", "Module rejects summary.");
                                isRestarting = true;
                            }
                            break;

                        default:
                            ad.mpLogger->warningv("CentralUnitAddressing Main", "Got unknow addressingState:", addressingState);
                            isRestarting = true;
                            break;
                    }

                    if (isRestarting) {
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
                CENTRAL_UNIT_ADDRESSING_TASK_SIZE,
                nullptr,
                MEDIUM_TASK_PRIORITY,
                &mAddressingTaskHandle
            );
        } else {
            mpLogger->warning("CentralUnitAddressing FreeRTOS", "Can't create addressing task, because task already exists.");
        }
    }

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

    // =================== Modules Addressing Data ====================
    void CentralUnitAddressing::printModulesAddressingData() const {
        if (!mpLogger->getIsSerialEnabled()) return;

        mpLogger->warning("CentralUnitAddressing Class", "Debug method printModulesAddressingData() call - not thread-safe print.");
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
        if (!mpLogger->getIsSerialEnabled()) return;

        mpLogger->warning("CentralUnitAddressing Class", "Debug method printNumOFModulesOnRfChannels() call - not thread-safe print.");
        xSemaphoreTake(mModulesAddressingDataMutex, portMAX_DELAY);
        for (uint8_t i = 0; i < MAX_CHANNEL; i++) {
            if (mNumOfModulesOnRfChannel[i] != 0) {
                char buffer[20];
                sprintf(
                    buffer,
                    "channel: %d num: %d",
                    (int)indexToRfChannel(i),
                    mNumOfModulesOnRfChannel[i]
                );
                Serial.println(buffer);
            }
        }
        xSemaphoreGive(mModulesAddressingDataMutex);
    }

    void CentralUnitAddressing::getModuleData(ModuleAddressingData *addressingData, const uint8_t ipAddress) const {
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
                    for (uint8_t channelIndex = 0; channelIndex < MAX_CHANNEL; channelIndex++) {
                        if (mNumOfModulesOnRfChannel[channelIndex] == 0) {
                            tmpChannel = channelIndex;
                            break;
                        } else if (mNumOfModulesOnRfChannel[channelIndex] < minNum) {
                            minNum = mNumOfModulesOnRfChannel[channelIndex];
                            tmpChannel = channelIndex;
                        }
                    }

                    rfChannel = indexToRfChannel(tmpChannel);
                }
                mNumOfModulesOnRfChannel[rfChannelToIndex(rfChannel)]++;

                // (temporary) save data about module
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
            mpLogger->error("CentralUnitAddressing AddModule", "Not found free IP address.");
        }
        return chosenIP;
    }

    void CentralUnitAddressing::removeModule(const uint8_t ipAddress) {
        xSemaphoreTake(mModulesAddressingDataMutex, portMAX_DELAY);

        const uint8_t index = ipToIndex(ipAddress);
        mNumOfModulesOnRfChannel[rfChannelToIndex(mModulesAddressingData[index].rfChannel)]--;

        mModulesAddressingData[index].isMACAddressReal = true;
        mModulesAddressingData[index].ipAddress = NULL_IP;
        mModulesAddressingData[index].rfChannel = DEFAULT_CHANNEL;
        uah::clearBuffer(mModulesAddressingData[index].macAddress, MAC_ADDRESS_LENGTH);
        mTmpModuleIp = NULL_IP;
        mIsStartOfAddressing = false;

        xSemaphoreGive(mModulesAddressingDataMutex);
        saveModulesAddressingData();
    }

    uint8_t CentralUnitAddressing::getTmpModuleIp() const {
        xSemaphoreTake(mModulesAddressingDataMutex, portMAX_DELAY);
        const uint8_t tmpModuleIp = mTmpModuleIp;
        xSemaphoreGive(mModulesAddressingDataMutex);

        return tmpModuleIp;
    }

    void CentralUnitAddressing::setTmpModuleIp(const uint8_t ip) {
        xSemaphoreTake(mModulesAddressingDataMutex, portMAX_DELAY);
        mTmpModuleIp = ip;
        xSemaphoreGive(mModulesAddressingDataMutex);
    }

    constexpr char CentralUnitAddressing::sm_JK_MODULES[];
    constexpr char CentralUnitAddressing::sm_JK_NUM_OF_MODULES_ON_RF_CHANNEL[];
    constexpr char CentralUnitAddressing::ModuleAddressingData::sm_JK_IP[];
    constexpr char CentralUnitAddressing::ModuleAddressingData::sm_JK_RF_CHANNEL[];
    constexpr char CentralUnitAddressing::ModuleAddressingData::sm_JK_MAC_ADDRESS[];
    constexpr char CentralUnitAddressing::ModuleAddressingData::sm_JK_IS_MAC_REAL[];

    void CentralUnitAddressing::loadModulesAddressingData() {
        const auto &dataManager = ums::DataManager::getInstance();
        const nl::json addressingData = dataManager.load(ADDRESSING_DATA_PATH);
        if (addressingData.empty()) return;

        xSemaphoreTake(mModulesAddressingDataMutex, portMAX_DELAY);
        for (uint8_t i = 0; i < MAX_CHANNEL; i++) {
            mNumOfModulesOnRfChannel[i] = addressingData[sm_JK_NUM_OF_MODULES_ON_RF_CHANNEL][i].get<uint8_t>();
        }
        uint8_t moduleIndex = 0;
        for (const auto& moduleDataJson : addressingData[sm_JK_MODULES]) {
            const ModuleAddressingData module(moduleDataJson);
            mModulesAddressingData[moduleIndex] = module;
            moduleIndex++;
        }
        xSemaphoreGive(mModulesAddressingDataMutex);
    }

    void CentralUnitAddressing::saveModulesAddressingData() {
        nl::json addressingDataJson;
        addressingDataJson[sm_JK_NUM_OF_MODULES_ON_RF_CHANNEL] = nl::json::array();

        xSemaphoreTake(mModulesAddressingDataMutex, portMAX_DELAY);
        // TODO consider making function in uah
        for (auto & i : mNumOfModulesOnRfChannel) {
            addressingDataJson[sm_JK_NUM_OF_MODULES_ON_RF_CHANNEL].push_back(i);
        }
        addressingDataJson[sm_JK_MODULES] = nl::json::array();
        for (auto & moduleData : mModulesAddressingData) {
            nl::json moduleDataJson = moduleData.toJson();
            addressingDataJson[sm_JK_MODULES].push_back(moduleDataJson);
        }
        xSemaphoreGive(mModulesAddressingDataMutex);

        const auto &dataManager = ums::DataManager::getInstance();
        dataManager.save(ADDRESSING_DATA_PATH, addressingDataJson);
    }

    CentralUnitAddressing::ModuleAddressingData::ModuleAddressingData(const nl::json &json) {
        ipAddress = json[sm_JK_IP];
        rfChannel = json[sm_JK_RF_CHANNEL];
        isMACAddressReal = json[sm_JK_IS_MAC_REAL];
        for (uint8_t i = 0; i < MAC_ADDRESS_LENGTH; i++) {
            macAddress[i] = json[sm_JK_MAC_ADDRESS][i].get<uint8_t>();
        }
    }

    nlohmann::json CentralUnitAddressing::ModuleAddressingData::toJson() {
        nl::json json;
        json[sm_JK_IP] = ipAddress;
        json[sm_JK_RF_CHANNEL] = rfChannel;
        json[sm_JK_IS_MAC_REAL] = isMACAddressReal;
        json[sm_JK_MAC_ADDRESS] = nl::json::array();
        for (uint8_t i = 0; i < MAC_ADDRESS_LENGTH; i++) {
            json[sm_JK_MAC_ADDRESS].push_back(macAddress[i]);
        }
        return json;
    }

    // ============================ Other =============================
    void CentralUnitAddressing::clearNewConnectionData() {
        mpLogger->info("CentralUnitAddressing Main", "Clearing new connection data.");
        const uint8_t tmpModuleIp = getTmpModuleIp();
        if (tmpModuleIp != NULL_IP) {
            removeModule(tmpModuleIp);
        }

        mpCommunication->changeRFChannel(DEFAULT_CHANNEL);
        vTaskDelay(pdMS_TO_TICKS(ADDRESSING_DELAY_BETWEEN_ATTEMPTS));
        xQueueReset(mAddressingQueue);
    }

    void CentralUnitAddressing::abortAddressing() {
        mpLogger->warning("CentralUnitAddressing Main", "Aborting addressing.");
        clearNewConnectionData();

        mpCommunication->stopAddressingAlgorithm();
    }

    bool CentralUnitAddressing::getIsStartOfAddressing() const {
        xSemaphoreTake(mModulesAddressingDataMutex, portMAX_DELAY);
        const bool result = mIsStartOfAddressing;
        xSemaphoreGive(mModulesAddressingDataMutex);
        return result;
    }

    void CentralUnitAddressing::setIsStartOfAddressing(const bool value) {
        xSemaphoreTake(mModulesAddressingDataMutex, portMAX_DELAY);
        mIsStartOfAddressing = value;
        xSemaphoreGive(mModulesAddressingDataMutex);
    }


}