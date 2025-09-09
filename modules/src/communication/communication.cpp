#include "communication.h"

#include <Arduino.h>
#include <HardwareSerial.h>
#include <memory>

#include "../utils/uint8_array_handlers.h"

#include "config/addressing_config.h"

namespace uah = Utils::ArrayHandlers;

namespace Comms {
    Communication* Communication::mspCommunication = nullptr;

    // ============================ Public ============================

    Communication &Communication::getInstance(DebugLED *debugLED, const std::shared_ptr<ul::Logger> &logger) {
        static Communication instance(debugLED, logger);
        return instance;
    }

    void Communication::startAddressingAlgorithm() const {
        mpDebugLED->createPairingBlinkTask();
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

    void Communication::startPinging() const {
        constexpr uint8_t notificationValue = START_PINGING_NOTIF;
        xQueueSend(mMainNotificationsQueue, &notificationValue, portMAX_DELAY);
    }

    void Communication::addByteToDecode(const uint8_t data) const {
        xQueueSend(mReceiveByteQueue, &data, portMAX_DELAY);
        vTaskResume(mDecodeMessageTaskHandle);
    }

    void Communication::sendMessage(const uint8_t message[MESSAGE_SIZE]) const {
        xQueueSend(mSendMessagesQueue, message, portMAX_DELAY);
        vTaskResume(mEncodeMessageTaskHandle);
    }

    void Communication::sendInternalMessage(const uint8_t message[MESSAGE_SIZE]) const {
        xQueueSend(mReceiveMessageQueue, message, portMAX_DELAY);
    }

    // ================================================================

    // ================== Constructor and Destructor ==================

    Communication::Communication(DebugLED *debugLED, const std::shared_ptr<ul::Logger> &logger) :
        mpConnection(&Connection::getInstance(this, logger)),
        #ifdef HC12_MODULE
            mpRfModule(new HC12(this, logger)),
        #else
            #error "Not implemented"
        #endif
        #ifdef CENTRAL_UNIT
            mpAddressing(new CentralUnitAddressing(this, logger))
        #else
            mpAddressing(new ModuleAddressing(this, logger))
        #endif
    {
        mspCommunication = this;
        mpDebugLED = debugLED;
        mpLogger = logger;

        mLastTransmittedMessageMutex = xSemaphoreCreateMutex();
        setLastTransmittedMessage();

        createCommunicationQueues();
        createCommunicationTimers();

        createTerminalInputTask();
        createDecodeMessageTask();
        createEncodeMessageTask();
        createCommunicationMainTask();

        mpLogger->info("Communication Class", "Communication initialized.");
    }

    Communication::~Communication() {
        deleteTerminalInputTask();
        deleteEncodeMessageTask();
        deleteDecodeMessageTask();
        deleteCommunicationMainTask();

        deleteCommunicationQueues();
        deleteCommunicationTimers();

        vSemaphoreDelete(mLastTransmittedMessageMutex);
    }

    // ================================================================

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
        if (mSendMessagesQueue == nullptr) {
            mSendMessagesQueue = xQueueCreate(MESSAGE_QUEUE_LEN, sizeof(uint8_t[MESSAGE_SIZE]));
        }
    }

    void Communication::deleteCommunicationQueues() {
        if (mSendMessagesQueue != nullptr) {
            vQueueDelete(mSendMessagesQueue);
            mSendMessagesQueue = nullptr;
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
    // ================================================================

    // ====================== Communication Main ======================

    void Communication::receivedMessageDecider(bool *isReadingRawMessage) {
        uint8_t buffer[MESSAGE_SIZE];
        // TODO consider changing if else statements to switch case
        if (xQueueReceive(mReceiveMessageQueue, buffer, 0) == pdTRUE) {
            // if it is "repeat" message repeat last transmitted message
            if (uah::areArraysEqual(buffer, (uint8_t*)REPEAT_MESSAGE, SPECIAL_MESSAGE_LEN)) {
                repeatLastTransmittedMessage();
            }
            // if it is "ping", reply to ping
            else if (uah::areArraysEqual(buffer, (uint8_t*)PING_MESSAGE, SPECIAL_MESSAGE_LEN)) {
                replyToPing();
            }
            // if it is "reping", reply to ping
            else if (uah::areArraysEqual(buffer, (uint8_t*)RE_PING_MESSAGE, SPECIAL_MESSAGE_LEN)) {
                xTimerStop(mPingTimeoutTimer, portMAX_DELAY);
                mpLogger->info("Communication Main", "Ping Success");
            }
            // TODO !BEFORE PULL REQUEST! check if addressing work properly after changing API
            // if is addressing message
            else if ((buffer[1] == (uint8_t)'A' && buffer[2] == (uint8_t)'D') || *isReadingRawMessage) {
                *isReadingRawMessage = false;
                mpAddressing->addMessage(buffer);
            }
            // if is HC12 command
            // TODO !BEFORE PULL REQUEST! decide what to do with HC12 commands and tcp
            #ifdef HC12_MODULE
                else if (buffer[0] == (uint8_t)'H' && buffer[1] == (uint8_t)'C') {
                    mpRfModule->setupHC12(buffer);
                }
            #endif
            else {
                mpLogger->warninga(
                    "Communication Main",
                    "Received custom message.\nIgnored message: ",
                    buffer,
                    MESSAGE_SIZE
                );
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
        if (uxQueueMessagesWaiting(mSendMessagesQueue) != 0) {
            mpLogger->debug("Communication Main", "vTaskResume(mEncodeMessageTaskHandle);");
            vTaskResume(mEncodeMessageTaskHandle);
        }

        // monitor incoming rf messages
        receivedMessageDecider(isReadingRawMessage);

        // delay for watchdog
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    void Communication::pingTimeoutNotifHandling(uint8_t *pingAttempts) const {
        mpLogger->info("Communication Main", "Ping Timeout");
        *pingAttempts++;

        if (*pingAttempts > PING_MAX_ATTEMPTS) {
            xTimerStop(mPingTimeoutTimer, portMAX_DELAY);
            *pingAttempts = 0;
            mpLogger->info("Communication Main", "No response to ping.");
        } else {
            transmitPing();
        }
    }

    void Communication::communicationMainTask(void* parameters) {
        auto &com = *mspCommunication;

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
                    com.setLastTransmittedMessage();
                    vTaskSuspend(com.mEncodeMessageTaskHandle);
                    break;

                case START_PINGING_NOTIF:
                    pingAttempts = 1;
                    com.transmitPing();
                    xTimerStart(com.mPingTimeoutTimer, portMAX_DELAY);
                    break;

                case PING_TIMEOUT_NOTIF:
                    com.pingTimeoutNotifHandling(&pingAttempts);
                    break;

                case READ_RAW_MESSAGE_NOTIF:
                    isReadingRawMessage = true;
                    xTaskNotify(com.mDecodeMessageTaskHandle, READ_RAW_MESSAGE_NOTIF, eSetValueWithOverwrite);
                    break;

                case STOP_ADDRESSING_ALGORITHM_NOTIF:
                    isReadingRawMessage = false;
                    com.mpDebugLED->deletePairingBlinkTask();
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
                2048,
                nullptr,
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
    // ================================================================

    // ======================== Decode Message ========================

    bool Communication::isCheckSumCorrect(const uint8_t message[PROTOCOL_SIZE]) {
        uint16_t checksum = 0;
        for (uint8_t i = 0; i < PROTOCOL_SIZE; i++) {
            checksum += message[i];
        }
        return checksum % CHECKSUM_MODULO == 0;
    }

    bool Communication::extractMessageFromProtocolBuffer(const uint8_t protocolBuffer[][PROTOCOL_SIZE], uint8_t *messageBuffer) {
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
        auto &com = *mspCommunication;

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
            if (!isReadingRawMessage) {
                com.transmitRepeatMessage();
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
                    // TODO remove #ifndef directive before merge with main
                    #ifndef COMMUNICATION_WITHOUT_SAVING_ADDRESSING
                    // if MAC is incorrect wait for possible rest of message and ignore it
                    else if (!com.mpAddressing->isMACPropper(protocolBuffer[protoBuffMessageIndex])) {
                        xTimerStop(com.mReceiveMessageTimeoutTimer, portMAX_DELAY);
                        // TODO !BEFORE PULL REQUEST! change log to debug or info
                        com.mpLogger->warning("Communication Decode", "Bad MAC");
                        vTaskDelay(pdMS_TO_TICKS(RECEIVE_MESSAGE_TIMEOUT));
                        resetProtocolBuffer();
                        xQueueReset(com.mReceiveByteQueue);
                    }
                    // TODO consider changing that approach (and "repeat" message logic in general) due to potential spam with multiple modules on one channel
                    // if IP is incorrect, check if is "repeat" message,
                    // if not wait for possible rest of message and ignore it, otherwise resend last message
                    else if (!com.mpAddressing->isIpPropper(protocolBuffer[protoBuffMessageIndex][PROTOCOL_IP_INDEX])) {
                        xTimerStop(com.mReceiveMessageTimeoutTimer, portMAX_DELAY);
                        if (uah::areArraysEqual(&protocolBuffer[protoBuffMessageIndex][PROTOCOL_MESSAGE_START_INDEX], (uint8_t*)REPEAT_MESSAGE, SPECIAL_MESSAGE_LEN)) {
                            resetProtocolBuffer();
                            com.repeatLastTransmittedMessage();
                        } else {
                            // TODO !BEFORE PULL REQUEST! change log to debug or info
                            com.mpLogger->warning("Communication Decode", "Bad IP");
                            vTaskDelay(pdMS_TO_TICKS(RECEIVE_MESSAGE_TIMEOUT));
                            resetProtocolBuffer();
                            xQueueReset(com.mReceiveByteQueue);
                        }
                    }
                    #endif
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

                        // if ack number is not correct
                        if (!com.mpConnection->handleReceivedMessage(messageBuffer)) {
                            handleIncorrectMessage(isRawMessage);
                            com.mpLogger->warning("Communication Decode", "Bad ack number");
                        }

                        // send decoded message to queue
                        if (isRawMessage) {
                            isRawMessage = false;
                            com.mpLogger->infoa("Communication Decode", "Received raw message: ", protocolBuffer[0], PROTOCOL_SIZE, false);
                            xQueueSend(com.mReceiveMessageQueue, protocolBuffer[0], portMAX_DELAY);
                        } else {
                            com.mpLogger->infoa("Communication Decode", "Received message: ", messageBuffer, MESSAGE_SIZE);
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
                2048,
                nullptr,
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
    // ================================================================

    // ======================== Encode Message ========================

    void Communication::prepareChecksum(uint8_t protocolBuffer[PROTOCOL_SIZE]) {
        protocolBuffer[PROTOCOL_CHECKSUM_INDEX] = 0;
        uint16_t checkSum = 0;
        for (uint8_t i = 0; i < PROTOCOL_SIZE; i++) {
            checkSum += (uint16_t)protocolBuffer[i];
        }
        checkSum = (CHECKSUM_MODULO - (checkSum % CHECKSUM_MODULO)) % CHECKSUM_MODULO;

        protocolBuffer[PROTOCOL_CHECKSUM_INDEX] = checkSum;
    }

    void Communication::encodeMessageTask(void *parameters) {
        auto &com = *mspCommunication;

        // prepare protocol buffer
        // [0-5{mac}, 6{ip}, 7{messagesQuantity}, 8-13{message}, 14{checksum}, 15{\0}]
        uint8_t protocolBuffer[PROTOCOL_SIZE];

        // TODO implement setting IP address for central unit
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
            // wait until the message appears in the queue and save message in local messageBuffer
            if (xQueueReceive(com.mSendMessagesQueue, &messageBuffer, pdMS_TO_TICKS(SUSPEND_TASK_TIME_LONG)) == pdTRUE) {
                // only for central unit, because modules IP is constant
                #ifdef CENTRAL_UNIT
                    // prepare place for IP address
                    protocolBuffer[PROTOCOL_IP_INDEX] = com.mpAddressing->getIPAddress();
                #endif

                int8_t messagesQuantity = 0;
                uint8_t messageIndex = 0;
                // calc messageQuantity
                const uint8_t messageLen = uah::calcLenOfDataInArray(messageBuffer, MESSAGE_SIZE);
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
                if (!uah::areArraysEqual(messageBuffer, (uint8_t*)REPEAT_MESSAGE, SPECIAL_MESSAGE_LEN)) {
                    com.setLastTransmittedMessage(messageBuffer);
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
                2048,
                nullptr,
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
// ================================================================

// =================== Terminal Input Message =====================
    #ifdef DEBUG_MODE
    void Communication::terminalInputTask(void *parameters) {
        const auto &com = *mspCommunication;

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
                if (buffer[index] != 13) {
                    index++;
                }

                // send message to Prepare Message To Send (protocol)
                if (buffer[index - 1] == (uint8_t)'\n') {
                    buffer[index - 1] = 0;

                    com.mpLogger->infoa("Communication Input", "Input: ", buffer, MESSAGE_SIZE);

                    // special debug commands
                    if (uah::areArraysEqual(buffer, (uint8_t*)"startping", 9)) {
                        constexpr uint8_t notificationValue = START_PINGING_NOTIF;
                        xQueueSend(com.mMainNotificationsQueue, &notificationValue, portMAX_DELAY);
                    } else if (uah::areArraysEqual(buffer, (uint8_t*)"readraw", 7)) {
                        constexpr uint8_t notificationValue = READ_RAW_MESSAGE_NOTIF;
                        xQueueSendToFront(com.mMainNotificationsQueue, &notificationValue, portMAX_DELAY);
                    }
                    #ifdef ESP32_BOARD
                        else if (uah::areArraysEqual(buffer, (uint8_t*)"reboot", 6)) {
                            com.mpLogger->warning("Communication Input", "Rebooting...");
                            ESP.restart();
                        }
                    #endif
                    // rest
                    else {
                        // check if is HC_12 command
                        #ifdef HC12_MODULE
                            if (buffer[0] == 'A' && buffer[1] == 'T') {
                                buffer[0] = 'H';
                                buffer[1] = 'C';
                                xQueueSend(com.mReceiveMessageQueue, &buffer, portMAX_DELAY);
                            } else {
                                xQueueSend(com.mSendMessagesQueue, &buffer, portMAX_DELAY);
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
                2048,
                nullptr,
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
        void Communication::createSendCustomMessageTask() {}
        void Communication::deleteSendCustomMessageTask() {}
    #endif
// ================================================================

// ============================ Timers ============================
    void Communication::communicationTimersCallbacks(TimerHandle_t xTimer){
        const auto &com = *mspCommunication;

        if (xTimer == com.mReceiveMessageTimeoutTimer) {
            com.mpLogger->debug("Communication Timers", "Message timeout.");
            constexpr uint8_t notificationValue = MESSAGE_TIMEOUT_NOTIF;
            xQueueSend(com.mMainNotificationsQueue, &notificationValue, portMAX_DELAY);
        } else if (xTimer == com.mReceiveByteTimeoutTimer) {
            xTimerStop(com.mReceiveMessageTimeoutTimer, portMAX_DELAY);
            com.mpLogger->debug("Communication Timers", "Byte timeout.");
            constexpr uint8_t notificationValue = BYTE_TIMEOUT_NOTIF;
            xQueueSend(com.mMainNotificationsQueue, &notificationValue, portMAX_DELAY);
        } else if (xTimer == com.mPingTimeoutTimer) {
            com.mpLogger->debug("Communication Timers", "Ping timeout.");
            constexpr uint8_t notificationValue = PING_TIMEOUT_NOTIF;
            xQueueSend(com.mMainNotificationsQueue, &notificationValue, portMAX_DELAY);
        }
    }

    void Communication::createCommunicationTimers() {
        if (mReceiveMessageTimeoutTimer == nullptr) {
            mReceiveMessageTimeoutTimer = xTimerCreate(
                "Receive Message Timeout",
                pdMS_TO_TICKS(RECEIVE_MESSAGE_TIMEOUT),
                pdFALSE,
                nullptr,
                communicationTimersCallbacks
            );
        }
        if (mReceiveByteTimeoutTimer == nullptr) {
            mReceiveByteTimeoutTimer = xTimerCreate(
                "Receive Byte Timeout",
                pdMS_TO_TICKS(RECEIVE_BYTE_TIMEOUT),
                pdFALSE,
                nullptr,
                communicationTimersCallbacks
            );
        }
        if (mPingTimeoutTimer == nullptr) {
            mPingTimeoutTimer = xTimerCreate(
                "Ping Timeout",
                pdMS_TO_TICKS(RECEIVE_MESSAGE_TIMEOUT),
                pdTRUE,
                nullptr,
                communicationTimersCallbacks
            );
        }
    }

    void Communication::deleteCommunicationTimers() {
        if (mPingTimeoutTimer != nullptr) {
            xTimerDelete(mPingTimeoutTimer, portMAX_DELAY);
            mPingTimeoutTimer = nullptr;
        }
        if (mReceiveByteTimeoutTimer != nullptr) {
            xTimerDelete(mReceiveByteTimeoutTimer, portMAX_DELAY);
            mReceiveByteTimeoutTimer = nullptr;
        }
        if (mReceiveMessageTimeoutTimer != nullptr) {
            xTimerDelete(mReceiveMessageTimeoutTimer, portMAX_DELAY);
            mReceiveMessageTimeoutTimer = nullptr;
        }
    }
// ================================================================

// ============================ Other =============================
    void Communication::setLastTransmittedMessage() {
        xSemaphoreTake(mLastTransmittedMessageMutex, portMAX_DELAY);
        uah::clearBuffer(mLastTransmittedMessage, MESSAGE_SIZE);
        mLastTransmittedMessageAttempts = 0;
        xSemaphoreGive(mLastTransmittedMessageMutex);
    }

    void Communication::setLastTransmittedMessage(const uint8_t message[MESSAGE_SIZE]) {
        xSemaphoreTake(mLastTransmittedMessageMutex, portMAX_DELAY);
        uah::prepareBuffer(mLastTransmittedMessage, message, MESSAGE_SIZE, MESSAGE_SIZE);
        xSemaphoreGive(mLastTransmittedMessageMutex);
    }

    void Communication::transmitRepeatMessage() const {
        uint8_t buffer[MESSAGE_SIZE];
        uah::prepareBuffer(buffer, (uint8_t*)REPEAT_MESSAGE, SPECIAL_MESSAGE_LEN, MESSAGE_SIZE);
        xQueueSend(mSendMessagesQueue, buffer, portMAX_DELAY);
    }

    void Communication::repeatLastTransmittedMessage() {
        xSemaphoreTake(mLastTransmittedMessageMutex, portMAX_DELAY);
        if (mLastTransmittedMessage[0] != 0) {
            xQueueSend(mSendMessagesQueue, mLastTransmittedMessage, portMAX_DELAY);
        }
        mLastTransmittedMessageAttempts++;
        xSemaphoreGive(mLastTransmittedMessageMutex);

        if (mLastTransmittedMessageAttempts > REPEAT_LAST_MESSAGE_MAX_ATTEMPTS) {
            setLastTransmittedMessage();
        }
    }

    void Communication::transmitPing() const {
        uint8_t buffer[MESSAGE_SIZE];
        uah::prepareBuffer(buffer, (uint8_t*)PING_MESSAGE, SPECIAL_MESSAGE_LEN, MESSAGE_SIZE);
        xQueueSend(mSendMessagesQueue, buffer, portMAX_DELAY);
    }

    void Communication::replyToPing() const {
        uint8_t buffer[MESSAGE_SIZE];
        uah::prepareBuffer(buffer, (uint8_t*)RE_PING_MESSAGE, SPECIAL_MESSAGE_LEN, MESSAGE_SIZE);
        xQueueSend(mSendMessagesQueue, buffer, portMAX_DELAY);
    }
// ================================================================
}