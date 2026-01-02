#include "communication.h"

#include <optional>

#include "utils/uint8_array_handlers.h"
#include "universal_module_system/power_manager/power_manager.h"
#include "universal_module_system/data_manager.h"
#include "api/command_handler.h"

namespace uah = Utils::ArrayHandlers;

namespace Comms {
    // ============================ Public ============================
    Communication &Communication::getInstance(const std::shared_ptr<ums::DebugLED> &debugLED, const std::shared_ptr<ul::Logger> &logger) {
        static Communication instance(debugLED, logger);
        return instance;
    }

    void Communication::startAddressingAlgorithm() const {
        mpAddressing->startAddressing();
    }

    void Communication::stopAddressingAlgorithm() const {
        constexpr uint8_t notificationValue = STOP_ADDRESSING_ALGORITHM_NOTIF;
        xQueueSendToFront(mMainNotificationsQueue, &notificationValue, portMAX_DELAY);
    }

    void Communication::needRawMessage() const {
        constexpr uint8_t notificationValue = READ_RAW_MESSAGE_NOTIF;
        xQueueSendToFront(mMainNotificationsQueue, &notificationValue, portMAX_DELAY);
    }

    void Communication::resetEncodeMessageTask() {
        taskENTER_CRITICAL(&mCriticalSectionMutex);
        deleteEncodeMessageTask();
        createEncodeMessageTask();
        taskEXIT_CRITICAL(&mCriticalSectionMutex);
    }

    void Communication::addByteToDecode(const uint8_t data) const {
        xQueueSend(mReceiveByteQueue, &data, portMAX_DELAY);
        vTaskResume(mDecodeMessageTaskHandle);
    }

    void Communication::sendMessage(const uint8_t message[MESSAGE_SIZE]) const {
        xQueueSend(mEncodeMessagesQueue, message, portMAX_DELAY);
        vTaskResume(mEncodeMessageTaskHandle);
    }

    void Communication::sendPriorityMessage(const uint8_t message[MESSAGE_SIZE]) const {
        xQueueSendToFront(mEncodeMessagesQueue, message, portMAX_DELAY);
        vTaskResume(mEncodeMessageTaskHandle);
    }

    void Communication::sendInternalMessage(const uint8_t message[MESSAGE_SIZE]) const {
        xQueueSend(mReceiveMessageQueue, message, portMAX_DELAY);
    }

    void Communication::changeRFChannel(const uint8_t channel) const {
        mpRfModule->firstChangeRFChannel(channel);
    }

    void Communication::waitAndDisableRfModule() const {
        mpRfModule->waitAndDisable();
    }

    void Communication::putRfModuleToSleep() const {
        mpRfModule->sleep();
    }

    void Communication::endConnection() const {
        uint8_t sendBuffer[MESSAGE_SIZE] = {};
        API::CommandHandler ch(API::commandTypes::END);
        ch.generateMessage(sendBuffer);
        sendMessage(sendBuffer);
        mpConnection->endConnection();
    }

    uint8_t Communication::getDefaultRfChannel() const {
        return mpAddressing->getDefaultRFChannel();
    }

    // ================== Constructor and Destructor ==================
    Communication::Communication(const std::shared_ptr<ums::DebugLED> &debugLED, const std::shared_ptr<ul::Logger> &logger) :
        mpDebugLED(debugLED),
        mpLogger(logger),
        #ifdef CENTRAL_UNIT
            mpAddressing(new CentralUnitAddressing(this, logger)),
        #else
            mpAddressing(new ModuleAddressing(this, logger)),
        #endif
        #ifdef HC12_MODULE
            mpRfModule(new HC12(this, logger))
        #else
            #error "Not implemented"
        #endif
    {
        mpConnection = &Connection::getInstance(this, mpAddressing, mpRfModule, logger);

        createCommunicationQueues();
        createCommunicationTimers();

        createTerminalInputTask();
        createDecodeMessageTask();
        createEncodeMessageTask();
        createCommunicationMainTask();

        mpLogger->verbose("Communication Class", "Communication initialized.");
    }

    Communication::~Communication() {
        deleteTerminalInputTask();
        deleteEncodeMessageTask();
        deleteDecodeMessageTask();
        deleteCommunicationMainTask();

        deleteCommunicationQueues();
        deleteCommunicationTimers();
    }

    // ============================ Queues ============================
    void Communication::createCommunicationQueues() {
        if (mMainNotificationsQueue == nullptr) {
            mMainNotificationsQueue = xQueueCreate(NOTIFICATIONS_QUEUE_SIZE, sizeof(uint8_t));
        }
        if (mReceiveMessageQueue == nullptr) {
            mReceiveMessageQueue = xQueueCreate(MESSAGE_QUEUE_LEN, sizeof(uint8_t[MESSAGE_SIZE]));
        }
        if (mReceiveByteQueue == nullptr) {
            mReceiveByteQueue = xQueueCreate(RECEIVE_BYTE_QUEUE_LEN, sizeof(uint8_t));
        }
        if (mEncodeMessagesQueue == nullptr) {
            mEncodeMessagesQueue = xQueueCreate(MESSAGE_QUEUE_LEN, sizeof(uint8_t[MESSAGE_SIZE]));
        }
    }

    void Communication::deleteCommunicationQueues() {
        if (mEncodeMessagesQueue != nullptr) {
            vQueueDelete(mEncodeMessagesQueue);
            mEncodeMessagesQueue = nullptr;
        }
        if (mReceiveByteQueue != nullptr) {
            vQueueDelete(mReceiveByteQueue);
            mReceiveByteQueue = nullptr;
        }
        if (mReceiveMessageQueue != nullptr) {
            vQueueDelete(mReceiveMessageQueue);
            mReceiveMessageQueue = nullptr;
        }
        if (mMainNotificationsQueue != nullptr) {
            vQueueDelete(mMainNotificationsQueue);
            mMainNotificationsQueue = nullptr;
        }
    }

    // ====================== Communication Main ======================
    void Communication::receivedMessageDecider(bool *isReadingRawMessage) {
        uint8_t buffer[MESSAGE_SIZE];

        if (xQueueReceive(mReceiveMessageQueue, buffer, 0) == pdTRUE) {
            // TODO !mm consider change isReadingRawMessage flag to mpAddressing->getIsAddressingInProgress()
            // TODO !mm change/remove addressing algorithm
            // if it is addressing message
            if ((buffer[0] == (uint8_t)'A' && buffer[1] == (uint8_t)'D') || *isReadingRawMessage) {
                *isReadingRawMessage = false;
                mpAddressing->addMessage(buffer);
            }
            // if is HC12 command
            #ifdef HC12_MODULE
                else if (buffer[0] == (uint8_t)'H' && buffer[1] == (uint8_t)'C') {
                    mpRfModule->setupHC12(buffer);
                }
            #endif
            else {
                mpConnection->messageDecider(buffer);
            }
            *isReadingRawMessage = false;
        }
    }

    void Communication::normalOperationHandling(bool *isReadingRawMessage) {
        // extra protection if somehow queues are not empty and corresponding task is suspended
        if (uxQueueMessagesWaiting(mReceiveByteQueue) != 0) {
            mpLogger->debug("Communication Main", "vTaskResume(mDecodeMessageTaskHandle);");
            vTaskResume(mDecodeMessageTaskHandle);
        }
        if (uxQueueMessagesWaiting(mEncodeMessagesQueue) != 0) {
            mpLogger->debug("Communication Main", "vTaskResume(mEncodeMessageTaskHandle);");
            vTaskResume(mEncodeMessageTaskHandle);
        }

        // monitor incoming rf messages
        receivedMessageDecider(isReadingRawMessage);

        // delay for watchdog
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    void Communication::communicationMainTask(void* parameters) {
        auto& com = *static_cast<Communication*>(parameters);

        uint8_t status = DEFAULT_STATUS_NOTIF;
        uint8_t pingAttempts = 0;
        bool isReadingRawMessage = false;
        for (;;) {
            // change status
            if (xQueueReceive(com.mMainNotificationsQueue, &status, 0) == pdFALSE) {
                // reset notifications status if there is no notifications
                status = DEFAULT_STATUS_NOTIF;
            }
            switch (status) {
                case DEFAULT_STATUS_NOTIF:
                    com.normalOperationHandling(&isReadingRawMessage);
                    break;

                case BYTE_TIMEOUT_NOTIF:
                    xTaskNotify(com.mDecodeMessageTaskHandle, BYTE_TIMEOUT_NOTIF, eSetValueWithOverwrite);
                    break;

                case MESSAGE_TIMEOUT_NOTIF:
                    xTaskNotify(com.mDecodeMessageTaskHandle, MESSAGE_TIMEOUT_NOTIF, eSetValueWithOverwrite);
                    break;

                case SUSPEND_DECODE_MESSAGE_TASK_NOTIF:
                    com.mpLogger->debug("Communication Main", "vTaskSuspend(com.mDecodeMessageTaskHandle);");
                    vTaskSuspend(com.mDecodeMessageTaskHandle);
                    break;

                case SUSPEND_ENCODE_MESSAGE_TASK_NOTIF:
                    com.mpLogger->debug("Communication Main", "vTaskSuspend(com.mEncodeMessageTaskHandle);");
                    vTaskSuspend(com.mEncodeMessageTaskHandle);
                    break;

                case READ_RAW_MESSAGE_NOTIF:
                    isReadingRawMessage = true;
                    xTaskNotify(com.mDecodeMessageTaskHandle, READ_RAW_MESSAGE_NOTIF, eSetValueWithOverwrite);
                    break;

                case STOP_ADDRESSING_ALGORITHM_NOTIF:
                    isReadingRawMessage = false;
                    com.mpDebugLED->deleteBlinkTask();
                    com.mpAddressing->stopAddressing();
                    break;

                default:
                    com.mpLogger->errorv("Communication Main", "Got unknow status. Received Status:", (int)status);
                    break;
            }
        }
    }

    void Communication::createCommunicationMainTask() {
        if (mCommunicationMainTaskHandle == nullptr) {
            xTaskCreate(
                communicationMainTask,
                "Communication Main",
                COMMUNICATION_MAIN_TASK_SIZE,
                this,
                BACKGROUND_TASK_PRIORITY,
                &mCommunicationMainTaskHandle
            );
        } else {
            mpLogger->warning("Communication FreeRTOS", "Can't create Communication Main task, because task already exists.");
        }
    }
    void Communication::deleteCommunicationMainTask() {
        if (mCommunicationMainTaskHandle != nullptr) {
            vTaskDelete(mCommunicationMainTaskHandle);
            mCommunicationMainTaskHandle = nullptr;
        }
    }

    // ======================== Decode Message ========================
    bool Communication::isCheckSumCorrect(const uint8_t message[PROTOCOL_SIZE]) const {
        uint16_t checksum = 0;
        for (uint8_t i = 0; i < PROTOCOL_SIZE; i++) {
            checksum += message[i];
        }
        return checksum % CHECKSUM_MODULO == 0;
    }

    bool Communication::extractMessageFromProtocolBuffer(const uint8_t protocolBuffer[][PROTOCOL_SIZE], uint8_t *messageBuffer) const {
        // prepare received message buffer
        uah::clearBuffer(messageBuffer, MESSAGE_SIZE);
        uint8_t messageIndex = 0;
        uint8_t messagesQuantity = UINT8_MAX;
        uint8_t protoBuffMessageIndex = 0;

        do {
            for (uint8_t i = PROTOCOL_MESSAGE_START_INDEX; i < (PROTOCOL_MESSAGE_START_INDEX + PROTOCOL_MESSAGE_LENGTH); i++) {
                messageBuffer[messageIndex] = protocolBuffer[protoBuffMessageIndex][i];
                messageIndex++;
                // protection against buffer overload (62 => 63 max buffer index -1 for \0)
                if (messageIndex > MAX_MESSAGE_INDEX - 1) {
                    break;
                }
            }

            // check packet loss
            if (messagesQuantity != UINT8_MAX && messagesQuantity != protocolBuffer[protoBuffMessageIndex][MESSAGES_QUANTITY_INDEX] + 1) {
                return false;
            }

            messagesQuantity = protocolBuffer[protoBuffMessageIndex][MESSAGES_QUANTITY_INDEX];
            protoBuffMessageIndex++;
        } while(messagesQuantity != 0);
        return true;
    }

    void Communication::decodeMessageTask(void *parameters) {
        const auto& com = *static_cast<Communication*>(parameters);

        // timeout status/cause
        uint32_t timeoutStatus = DEFAULT_STATUS_NOTIF;
        // if true decodeMessageTask() will put in xQueueSend() protocolBuffer[0] instead messageBuffer
        bool isRawMessage = false;

        // prepare message protocol buffer
        // [0-5{mac}, 6{ip}, 7{messagesQuantity}, 8-13{message}, 14{checksum}, 15{\0}]
        uint8_t protocolBuffer[PROTOCOL_MESSAGE_MAX_NUM][PROTOCOL_SIZE];

        uint8_t protoBuffMessageIndex = 0;
        uint8_t protoBuffByteIndex = 0;

        auto resetProtocolBuffer = [&]() {
            for (uint8_t i = 0; i < PROTOCOL_MESSAGE_MAX_NUM; i++){
                uah::clearBuffer(protocolBuffer[i], PROTOCOL_SIZE);
            }
            protoBuffByteIndex = 0;
            protoBuffMessageIndex = 0;
        };
        resetProtocolBuffer();

        auto handleIncorrectMessage = [&](const bool isReadingRawMessage) {
            xTimerStop(com.mReceiveMessageTimeoutTimer, portMAX_DELAY);
            // wait for possible rest of the message and send "repeat"
            vTaskDelay(RECEIVE_MESSAGE_TIMEOUT);
            resetProtocolBuffer();
            xQueueReset(com.mReceiveByteQueue);
            // TODO change?
            if (!isReadingRawMessage) {
                com.mpConnection->transmitRepeatMessage();
            }
        };

        // buffer for byte received from rfModule
        uint8_t queueBuffer;

        // task loop
        for (;;) {
            // TODO consider adding method for handling notifications (it require to change lambda functions to class methods)
            // notifications handling
            if (xTaskNotifyWait(0, ULONG_MAX, &timeoutStatus, 0) == pdTRUE) {
                if (timeoutStatus == READ_RAW_MESSAGE_NOTIF) {
                    isRawMessage = true;
                    com.mpLogger->debug("Communication Decode", "Reading raw message.");
                } else {
                    if (timeoutStatus == BYTE_TIMEOUT_NOTIF) {
                        xTimerStop(com.mReceiveMessageTimeoutTimer, portMAX_DELAY);
                        com.mpLogger->warning("Communication Decode", "Byte timeout.");
                        resetProtocolBuffer();
                        xQueueReset(com.mReceiveByteQueue);
                    } else if (timeoutStatus == MESSAGE_TIMEOUT_NOTIF) {
                        xTimerStop(com.mReceiveByteTimeoutTimer, portMAX_DELAY);
                        com.mpLogger->warning("Communication Decode", "Message timeout.");
                        resetProtocolBuffer();
                        xQueueReset(com.mReceiveByteQueue);
                    } else {
                        com.mpLogger->errorv("Communication Decode", "Got unknow notification status. Received Status: ", (int)timeoutStatus);
                    }
                    isRawMessage = false;
                }
                timeoutStatus = DEFAULT_STATUS_NOTIF;
            }

            // decoding message
            if (xQueueReceive(com.mReceiveByteQueue, &queueBuffer, pdMS_TO_TICKS(SUSPEND_TASK_TIME_LONG)) == pdTRUE) {
                protocolBuffer[protoBuffMessageIndex][protoBuffByteIndex] = queueBuffer;
                // if new message start message timeout timer
                if (protoBuffByteIndex == 0 && protoBuffMessageIndex == 0){
                    xTimerStart(com.mReceiveMessageTimeoutTimer, portMAX_DELAY);
                }
                // if protocol message is not complete
                if (protoBuffByteIndex != PROTOCOL_SIZE - 1) {
                    protoBuffByteIndex++;
                    xTimerStart(com.mReceiveByteTimeoutTimer, portMAX_DELAY);
                } else {
                    xTimerStop(com.mReceiveByteTimeoutTimer, portMAX_DELAY);
                    protoBuffByteIndex = 0;

                    // if message is not end properly
                    const uint8_t endOfMessage = protocolBuffer[protoBuffMessageIndex][PROTOCOL_SIZE - 1];
                    if (endOfMessage != 0) {
                        handleIncorrectMessage(isRawMessage);
                        com.mpLogger->warningv("Communication Decode", "Bad end of message. Message should end with 0 (\\0 char), but got: ", endOfMessage);
                    }
                    // if checksum is incorrect
                    else if (!com.isCheckSumCorrect(protocolBuffer[protoBuffMessageIndex])) {
                        handleIncorrectMessage(isRawMessage);
                        com.mpLogger->warning("Communication Decode", "Bad checksum");
                    }
                    // if MAC is incorrect wait for possible rest of message and ignore it
                    else if (!com.mpAddressing->isMACValid(protocolBuffer[protoBuffMessageIndex])) {
                        xTimerStop(com.mReceiveMessageTimeoutTimer, portMAX_DELAY);
                        com.mpLogger->debug("Communication Decode", "Bad MAC");
                        vTaskDelay(pdMS_TO_TICKS(RECEIVE_MESSAGE_TIMEOUT));
                        resetProtocolBuffer();
                        xQueueReset(com.mReceiveByteQueue);
                    }
                    // if IP is incorrect wait for possible rest of message and ignore it
                    else if (!com.mpAddressing->isIpValid(protocolBuffer[protoBuffMessageIndex][PROTOCOL_IP_INDEX])) {
                        xTimerStop(com.mReceiveMessageTimeoutTimer, portMAX_DELAY);
                        com.mpLogger->debugv("Communication Decode", "Bad IP: ", protocolBuffer[protoBuffMessageIndex][PROTOCOL_IP_INDEX]);
                        vTaskDelay(pdMS_TO_TICKS(RECEIVE_MESSAGE_TIMEOUT));
                        resetProtocolBuffer();
                        xQueueReset(com.mReceiveByteQueue);
                    }
                    // if entire message is not ready (message quantity)
                    else if (protocolBuffer[protoBuffMessageIndex][MESSAGES_QUANTITY_INDEX] != 0) {
                        protoBuffMessageIndex++;
                    }
                    // entire message is ready
                    else {
                        xTimerStop(com.mReceiveMessageTimeoutTimer, portMAX_DELAY);
                        uint8_t messageBuffer[MESSAGE_SIZE];

                        // if packet loss
                        if (!com.extractMessageFromProtocolBuffer(protocolBuffer, messageBuffer)) {
                            com.mpLogger->warning("Communication Decode", "Lost packet.");
                            handleIncorrectMessage(isRawMessage);
                            continue;
                        }

                        com.mpConnection->receivingHandle(protocolBuffer[0][PROTOCOL_IP_INDEX]);

                        // send decoded message to queue
                        if (isRawMessage) {
                            isRawMessage = false;
                            com.mpLogger->debuga("Communication Decode", "Received raw message: ", protocolBuffer[0], PROTOCOL_SIZE, false);
                            xQueueSend(com.mReceiveMessageQueue, protocolBuffer[0], portMAX_DELAY);
                        } else {
                            com.mpLogger->debuga("Communication Decode", "Received message: ", messageBuffer, MESSAGE_SIZE);
                            xQueueSend(com.mReceiveMessageQueue, messageBuffer, portMAX_DELAY);
                        }

                        // clean up
                        resetProtocolBuffer();
                    }
                }
            } else {
                constexpr uint8_t notificationValue = SUSPEND_DECODE_MESSAGE_TASK_NOTIF;
                xQueueSend(com.mMainNotificationsQueue, &notificationValue, portMAX_DELAY);
            }
        }
    }

    void Communication::createDecodeMessageTask() {
        if (mDecodeMessageTaskHandle == nullptr) {
            xTaskCreate(
                decodeMessageTask,
                "Decode message",
                DECODE_TASK_SIZE,
                this,
                LOW_TASK_PRIORITY,
                &mDecodeMessageTaskHandle
            );
        } else {
            mpLogger->warning("Communication FreeRTOS", "Can't create Decode task, because task already exists.");
        }
    }
    void Communication::deleteDecodeMessageTask() {
        if (mDecodeMessageTaskHandle != nullptr) {
            vTaskDelete(mDecodeMessageTaskHandle);
            mDecodeMessageTaskHandle = nullptr;
        }
    }

    // ======================== Encode Message ========================
    void Communication::prepareChecksum(uint8_t protocolBuffer[PROTOCOL_SIZE]) {
        protocolBuffer[PROTOCOL_CHECKSUM_INDEX] = 0;
        uint16_t checkSum = 0;
        for (uint8_t i = 0; i < PROTOCOL_SIZE; i++) {
            checkSum += (uint16_t)protocolBuffer[i];
        }

        #ifdef TEST_CHECKSUM
            // force bad checksum
            pinMode(18, INPUT_PULLUP);
            if (digitalRead(18) == LOW) {
                checkSum++;
            }
        #endif

        checkSum = (CHECKSUM_MODULO - (checkSum % CHECKSUM_MODULO)) % CHECKSUM_MODULO;

        protocolBuffer[PROTOCOL_CHECKSUM_INDEX] = checkSum;
    }

    void Communication::encodeMessageTask(void *parameters) {
        auto& com = *static_cast<Communication*>(parameters);

        // prepare protocol buffer
        // [0-5{mac}, 6{ip}, 7{messagesQuantity}, 8-13{message}, 14{checksum}, 15{\0}]
        uint8_t protocolBuffer[PROTOCOL_SIZE];

        // prepare MAC address in protocol buffer and clear rest of buffer
        uint8_t macAddress[6];
        com.mpAddressing->getProtocolMACAddress(macAddress);
        uah::prepareBuffer(protocolBuffer, macAddress, MAC_ADDRESS_LENGTH, PROTOCOL_SIZE);
        // prepare place for IP address
        protocolBuffer[PROTOCOL_IP_INDEX] = com.mpAddressing->getIPAddress();

        // prepare place for message
        for (uint8_t i = PROTOCOL_MESSAGE_START_INDEX; i < (PROTOCOL_MESSAGE_START_INDEX + PROTOCOL_MESSAGE_LENGTH); i++) {
            protocolBuffer[i] = BLANK_CHARACTER;
        }

        // prepare message to send buffer
        uint8_t messageBuffer[MESSAGE_SIZE];
        uah::clearBuffer(messageBuffer, MESSAGE_SIZE);

        // task loop
        for (;;) {
            // wait until the message appears in the queue and saveJson message in local messageBuffer
            if (xQueueReceive(com.mEncodeMessagesQueue, &messageBuffer, pdMS_TO_TICKS(SUSPEND_TASK_TIME_LONG)) == pdTRUE) {
                com.mpConnection->sendingHandle(messageBuffer);
                // only for central unit, because modules IP is constant
                #ifdef CENTRAL_UNIT
                    // prepare place for IP address
                    protocolBuffer[PROTOCOL_IP_INDEX] = com.mpAddressing->getIPAddress();
                #endif

                int8_t messagesQuantity = 0;
                uint8_t messageIndex = 0;

                // calc messageQuantity
                uint8_t messageLen = MESSAGE_SIZE - 1;
                while (messageBuffer[messageLen] == '\0') {
                    messageLen--;
                }
                messageLen++;
                messagesQuantity = messageLen / PROTOCOL_MESSAGE_LENGTH;
                if (messageLen % PROTOCOL_MESSAGE_LENGTH == 0) {
                    messagesQuantity--;
                }

                // put part of message in protocolBuffer and send it to transmitting task
                for (messagesQuantity; messagesQuantity >= 0; messagesQuantity--) {
                    protocolBuffer[MESSAGES_QUANTITY_INDEX] = messagesQuantity;
                    for (uint8_t i = 0; i < PROTOCOL_MESSAGE_LENGTH; i++) {
                        protocolBuffer[PROTOCOL_MESSAGE_START_INDEX + i] = messageBuffer[messageIndex] != 0 ? messageBuffer[messageIndex] : BLANK_CHARACTER;
                        messageIndex++;
                    }
                    // calculate and set checksum
                    com.prepareChecksum(protocolBuffer);
                    com.mpRfModule->addMessageToTransmit(protocolBuffer);

                    com.mpLogger->debuga("Communication Encode", "Protocol buffer: ", protocolBuffer, PROTOCOL_SIZE, false);
                    com.mpLogger->debuga("Communication Encode", "Protocol message: ", &protocolBuffer[PROTOCOL_MESSAGE_START_INDEX], PROTOCOL_MESSAGE_LENGTH);
                }
            } else {
                constexpr uint8_t notificationValue = SUSPEND_ENCODE_MESSAGE_TASK_NOTIF;
                xQueueSend(com.mMainNotificationsQueue, &notificationValue, portMAX_DELAY);
            }
        }
    }

    void Communication::createEncodeMessageTask() {
        if (mEncodeMessageTaskHandle == nullptr) {
            xTaskCreate(
                encodeMessageTask,
                "Encode Message",
                ENCODE_TASK_SIZE,
                this,
                MEDIUM_TASK_PRIORITY,
                &mEncodeMessageTaskHandle
            );
        } else {
            mpLogger->warning("Communication FreeRTOS", "Can't create Encode task, because task already exists.");
        }
    }

    void Communication::deleteEncodeMessageTask() {
        if (mEncodeMessageTaskHandle != nullptr) {
            vTaskDelete(mEncodeMessageTaskHandle);
            mEncodeMessageTaskHandle = nullptr;
        }
    }

    // =================== Terminal Input Message =====================
    #ifdef DEBUG_MODE
    void Communication::terminalInputTask(void *parameters) {
        const auto& com = *static_cast<Communication*>(parameters);

        // prepare buffer
        uint8_t buffer[MESSAGE_SIZE];
        for (uint8_t i = 0; i < MESSAGE_SIZE; i++){
            buffer[i] = 0;
        }
        uint8_t index = 0;

        // task loop
        for(;;) {
            if (Serial.available() > 0) {
                buffer[index] = Serial.read();
                com.mpLogger->printInputChar(buffer[index]);
                if (buffer[index] == '\b') {
                    if (index > 0) {
                        index--;
                    }
                } else if (buffer[index] != '\r') {
                    index++;
                }

                // send message to Prepare Message To Send (protocol)
                if (buffer[index - 1] == (uint8_t)'\n') {
                    buffer[index - 1] = 0;

                    com.mpLogger->infoa("Communication Input", "Input: ", buffer, MESSAGE_SIZE);

                    // special debug commands
                    if (uah::areArraysEqual(buffer, "readraw")) {
                        constexpr uint8_t notificationValue = READ_RAW_MESSAGE_NOTIF;
                        xQueueSendToFront(com.mMainNotificationsQueue, &notificationValue, portMAX_DELAY);
                    }
                    #ifdef ESP32_BOARD
                        else if (uah::areArraysEqual(buffer, "reboot")) {
                            auto & powerManager = ums::PowerManager::getInstance(com.mpLogger);
                            powerManager.safeRestart("Communication Input");
                        }
                    #endif
                    else if (uah::areArraysEqual(buffer, "ls")) {
                        auto & dataManager = ums::DataManager::getInstance();
                        dataManager.ls();
                    } else if (uah::areArraysEqual(buffer, "cat")) {
                        auto & dataManager = ums::DataManager::getInstance();
                        dataManager.cat((char*)&buffer[strlen("cat") + 1]);
                    } else if (uah::areArraysEqual(buffer, "rm")) {
                        auto & dataManager = ums::DataManager::getInstance();
                        dataManager.rm((char*)&buffer[strlen("rm") + 1]);
                    } else if (uah::areArraysEqual(buffer, "ip=")) {
                        std::optional<uint8_t> ip;
                        try {
                            ip = std::stoi((char*)&buffer[3]);
                        } catch (...) {
                            ip.reset();
                        }
                        if (ip.has_value()) {
                            com.mpAddressing->setProtocolIPAddress(ip.value());
                        } else {
                            com.mpLogger->error("Communication Input", "Bad IP");
                        }
                    } else if (uah::areArraysEqual(buffer, "end")) {
                        com.endConnection();
                    } else if (uah::areArraysEqual(buffer, "test")) {
                        API::CommandHandler ch(API::commandTypes::GET);
                        std::optional<uint8_t> getType;
                        try {
                            char buffer2[MESSAGE_SIZE];
                            std::memcpy(buffer2,buffer,MESSAGE_SIZE);
                            getType = std::stoi((char*)&buffer[4]);
                        } catch (...) {
                            com.mpLogger->error("Communication Input", "Bad getType");
                        }

                        if (getType.has_value()) {
                            constexpr uint32_t TEST_UID = 88888;
                            API::APIParameter uid(TEST_UID);
                            API::APIParameter getTypeParam(getType.value());
                            ch.addParameter(uid);
                            ch.addParameter(getTypeParam);
                            if (getType.value() == (uint8_t)API::getTypes::BATTERY_STATE) {
                                constexpr uint8_t BATTERY_ID = 1;
                                ch.addParameter(API::APIParameter(BATTERY_ID));
                            } else if (
                                getType.value() == (uint8_t)API::getTypes::SENSOR_VALUE ||
                                getType.value() == (uint8_t)API::getTypes::SENSOR_VALUE_WITH_FORCE_NEW_READING
                            ) {
                                std::optional<uint8_t> sensorId;
                                try {
                                    sensorId = std::stoi((char*)&buffer[6]);
                                } catch (...) {
                                    com.mpLogger->error("Communication Input", "Bad sensorId");
                                }
                                if (sensorId.has_value()) {
                                    ch.addParameter(API::APIParameter(sensorId.value()));
                                }
                            }

                            uint8_t message[MESSAGE_SIZE] = {};
                            ch.generateMessage(message);
                            xQueueSend(com.mEncodeMessagesQueue, &message, portMAX_DELAY);
                        }
                    }
                    // sleep command
                    else if (uah::areArraysEqual(buffer, "sl")) {
                        std::optional<uint32_t> sleepTime;
                        try {
                            sleepTime = std::stoi((char*)&buffer[2]);
                            com.mpLogger->errorv("Communication Input", "good sleep time:", sleepTime.value());
                        } catch (...) {
                            com.mpLogger->error("Communication Input", "Bad sleep time.");
                        }

                        if (sleepTime.has_value()) {
                            API::CommandHandler ch(API::commandTypes::SLEEP);
                            ch.addParameter(API::APIParameter<uint32_t>(88888));
                            ch.addParameter(API::APIParameter(sleepTime.value()));
                            uint8_t message[MESSAGE_SIZE] = {};
                            ch.generateMessage(message);
                            com.sendMessage(message);
                        }
                    }
                    // deep sleep command
                    else if (uah::areArraysEqual(buffer, "dsl")) {
                        std::optional<uint32_t> sleepTime;
                        try {
                            sleepTime = std::stoi((char*)&buffer[3]);
                        } catch (...) {
                            com.mpLogger->error("Communication Input", "Bad sleep time.");
                        }

                        if (sleepTime.has_value()) {
                            API::CommandHandler ch(API::commandTypes::DEEP_SLEEP);
                            ch.addParameter(API::APIParameter<uint32_t>(88888));
                            ch.addParameter(API::APIParameter(sleepTime.value()));
                            uint8_t message[MESSAGE_SIZE] = {};
                            ch.generateMessage(message);
                            com.sendMessage(message);
                        }
                    }
                    // notif test
                    else if (uah::areArraysEqual(buffer, "notif")) {
                        API::CommandHandler ch(API::commandTypes::NOTIFY);
                        ch.addParameter(API::APIParameter((uint8_t)1));
                        uint8_t message[MESSAGE_SIZE] = {};
                        ch.generateMessage(message);
                        com.sendMessage(message);
                    } else if (uah::areArraysEqual(buffer, "ping")) {
                        API::CommandHandler ch(API::commandTypes::PING);
                        ch.addParameter(API::APIParameter((uint8_t)12));
                        uint8_t message[MESSAGE_SIZE] = {};
                        ch.generateMessage(message);
                        com.sendMessage(message);
                    }
                    // ascii test
                    else if (uah::areArraysEqual(buffer, "ascii")) {
                        API::CommandHandler ch(API::commandTypes::RESPONSE);
                        char text[] = "abcdefg";
                        ch.addParameter(API::APIParameter(text, strlen(text)));
                        uint8_t message[MESSAGE_SIZE] = {};
                        ch.generateMessage(message);
                        com.sendMessage(message);
                    }
                    // rest
                    else {
                        // check if is HC_12 command
                        #ifdef HC12_MODULE
                            if (buffer[0] == 'A' && buffer[1] == 'T') {
                                buffer[0] = 'H';
                                buffer[1] = 'C';
                                xQueueSend(com.mReceiveMessageQueue, &buffer, portMAX_DELAY);
                            } else {
                                xQueueSend(com.mEncodeMessagesQueue, &buffer, portMAX_DELAY);
                            }
                        #else
                            #error "not implemented"
                        #endif
                    }
                    // reset buffer
                    uah::clearBuffer(buffer, index);
                    index = 0;
                }
            }
            // delay for watchdog
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    void Communication::createTerminalInputTask() {
        if (mTerminalInputTaskHandle == nullptr) {
            xTaskCreate(
                terminalInputTask,
                "Terminal Input Task",
                TERMINAL_INPUT_TASK_SIZE,
                this,
                BACKGROUND_TASK_PRIORITY,
                &mTerminalInputTaskHandle
            );
        } else {
            mpLogger->warning("Communication FreeRTOS", "Can't create Terminal Input task, because task already exists.");
        }
    }

    void Communication::deleteTerminalInputTask() {
        if (mTerminalInputTaskHandle != nullptr) {
            vTaskDelete(mTerminalInputTaskHandle);
            mTerminalInputTaskHandle = nullptr;
        }
    }

    #else
        void Communication::createTerminalInputTask() {}
        void Communication::deleteTerminalInputTask() {}
    #endif

    // ============================ Timers ============================
    void Communication::communicationTimersCallbacks(TimerHandle_t xTimer){
        const auto &com = *static_cast<Communication*>(pvTimerGetTimerID(xTimer));

        if (xTimer == com.mReceiveMessageTimeoutTimer) {
            com.mpLogger->debug("Communication Timers", "Message timeout.");
            constexpr uint8_t notificationValue = MESSAGE_TIMEOUT_NOTIF;
            xQueueSend(com.mMainNotificationsQueue, &notificationValue, portMAX_DELAY);
        } else if (xTimer == com.mReceiveByteTimeoutTimer) {
            xTimerStop(com.mReceiveMessageTimeoutTimer, portMAX_DELAY);
            com.mpLogger->debug("Communication Timers", "Byte timeout.");
            constexpr uint8_t notificationValue = BYTE_TIMEOUT_NOTIF;
            xQueueSend(com.mMainNotificationsQueue, &notificationValue, portMAX_DELAY);
        }
    }

    void Communication::createCommunicationTimers() {
        if (mReceiveMessageTimeoutTimer == nullptr) {
            mReceiveMessageTimeoutTimer = xTimerCreate(
                "Receive Message Timeout",
                pdMS_TO_TICKS(RECEIVE_MESSAGE_TIMEOUT),
                pdFALSE,
                this,
                communicationTimersCallbacks
            );
        }
        if (mReceiveByteTimeoutTimer == nullptr) {
            mReceiveByteTimeoutTimer = xTimerCreate(
                "Receive Byte Timeout",
                pdMS_TO_TICKS(RECEIVE_BYTE_TIMEOUT),
                pdFALSE,
                this,
                communicationTimersCallbacks
            );
        }
    }

    void Communication::deleteCommunicationTimers() {
        if (mReceiveByteTimeoutTimer != nullptr) {
            xTimerDelete(mReceiveByteTimeoutTimer, portMAX_DELAY);
            mReceiveByteTimeoutTimer = nullptr;
        }
        if (mReceiveMessageTimeoutTimer != nullptr) {
            xTimerDelete(mReceiveMessageTimeoutTimer, portMAX_DELAY);
            mReceiveMessageTimeoutTimer = nullptr;
        }
    }
}