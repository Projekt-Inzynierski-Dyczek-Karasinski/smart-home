#include "communication.h"
// #include "communication/communication.h"

#include <Arduino.h>
#include <HardwareSerial.h>

#include "uint8_array_handlers.h"
#include "universal_module_system/debug_led.h"

#include "smart_home_config.h"
#include "config/communication_config.h"
#include "config/addressing_config.h"

#ifdef HC12_MODULE
    #include "hc12.h"
#else
    #error "Not implemented" 
#endif

#ifdef CENTRAL_UNIT 
    #include "communication/addressing/central_unit_addressing.h"
#else
    #include "addressing/module_addressing.h"
#endif

namespace uah = uint8ArrayHandlers;

DebugLED* Communication::mspDebugLED = nullptr;

// ============================ Public ============================

Communication &Communication::getInstance(DebugLED *debugLED) {
    static Communication instance(debugLED);
    return instance;
}

void Communication::startAddressingAlgorithm() const {
    mspDebugLED->createPairingBlinkTask();
    mpAddressing->startAddressing();
}

void Communication::stopAddressingAlgorithm() const {
    xTaskNotify(mCommunicationMainTaskHandle, STOP_ADDRESING_ALGORITHM_NOTIF, eSetValueWithOverwrite);
}

void Communication::needRawMessage() const {
    xTaskNotify(mCommunicationMainTaskHandle, READ_RAW_MESSAGE_NOTIF, eSetValueWithOverwrite);
}

void Communication::resetEncodeMessageTask() {
    taskENTER_CRITICAL(&mCriticalSectionMutex);
    deleteEncodeMessageTask();
    createEncodeMessageTask();
    taskEXIT_CRITICAL(&mCriticalSectionMutex);
}

void Communication::startPinging() const {
    xTaskNotify(mCommunicationMainTaskHandle, START_PINGING_NOTIF, eSetValueWithOverwrite);
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

Communication::Communication(DebugLED *debugLED) :
    #ifdef HC12_MODULE
        mpRfModule(new HC12(this)),
    #else
        #error "Not implemented" 
    #endif
    #ifdef CENTRAL_UNIT 
        mpAddressing(new CentralUnitAddressing(this))
    #else
        mpAddressing(new ModuleAddressing(this))
    #endif
{
    mspDebugLED = debugLED;

    mLastTransmittedMessageMutex = xSemaphoreCreateMutex();
    setLastTransmittedMessage();

    createCommunicationQueues();
    createCommunicationTimers();
    
    createSendCustomMessageTask();
    createDecodeMessageTask();
    createEncodeMessageTask();
    createCommunicationMainTask();

    Serial.println("Communication initialized"); 
}

Communication::~Communication() {
    deleteSendCustomMessageTask();
    deleteEncodeMessageTask();
    deleteDecodeMessageTask();
    deleteCommunicationMainTask();

    deleteCommunicationQueues();
    deleteCommunicationTimers();
}

// ================================================================

// ============================ Queues ============================

void Communication::createCommunicationQueues() {
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
    if (mReceiveMessageQueue != nullptr) {
        vQueueDelete(mReceiveMessageQueue);
        mReceiveMessageQueue = nullptr;
    }
    if (mReceiveByteQueue != nullptr) {
        vQueueDelete(mReceiveByteQueue);
        mReceiveByteQueue = nullptr;
    }
    if (mSendMessagesQueue != nullptr) {
        vQueueDelete(mSendMessagesQueue);
        mSendMessagesQueue = nullptr;
    }
}
// ================================================================

// ====================== Communication Main ======================

void Communication::receivedMessageDecider(bool *isReadingRawMessage) {
    uint8_t buffer[MESSAGE_SIZE];
    // TODO consider changing if else statements to switch case
    if (xQueueReceive(mReceiveMessageQueue, buffer, 0) == pdTRUE) {
        // if it is "repeat" message repeat last transmitted message
        if (uah::areArraysEqual(buffer, (uint8_t*)"repeat", 6)) {
            repeatLastTransmittedMessage();
        } 
        // if it is "ping", reply to ping
        else if (uah::areArraysEqual(buffer, (uint8_t*)"ping", 4)) {
            replyToPing();
        }
        // if it is "reping", reply to ping
        else if (uah::areArraysEqual(buffer, (uint8_t*)"reping", 6)) {
            xTimerStop(mPingTimeoutTimer, portMAX_DELAY);
            Serial.println("Ping Success");
        }
        // if is addressing message
        else if ((buffer[0] == (uint8_t)'A' && buffer[1] == (uint8_t)'D') || *isReadingRawMessage) {
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
            Serial.println("COMMUNICATION WARNING! In receivedMessageDecider() -> decider doesn't know what to do with received message");
            Serial.print("Ignored message: ");
            uah::printArrayAsChar(buffer, MESSAGE_SIZE);
        }
        *isReadingRawMessage = false;
    }
}

void Communication::normalOperationHandling(bool *isReadingRawMessage) {
    // extra protection if somehow queues are not empty and corresponding task is suspended 
    if (uxQueueMessagesWaiting(mReceiveByteQueue) != 0) {
        vTaskResume(mDecodeMessageTaskHandle);
    }
    if (uxQueueMessagesWaiting(mSendMessagesQueue) != 0) {
        vTaskResume(mEncodeMessageTaskHandle);
    }

    // monitor incoming rf messages
    receivedMessageDecider(isReadingRawMessage);

    // delay for watchdog
    vTaskDelay(pdMS_TO_TICKS(1));
}

void Communication::pingTimeoutNotifHandling(uint8_t *pingAttempts) const {
    Serial.println("Ping Timeout");
    *pingAttempts++;

    if (*pingAttempts > PING_MAX_ATTEMPTS) {
        xTimerStop(mPingTimeoutTimer, portMAX_DELAY);
        *pingAttempts = 0;
        Serial.println("PING ERROR! In pingTimeoutNotifHandling() -> no response to ping.");
    } else {
        transmitPing();
    }
}

void Communication::communicationMainTask(void* parameters) {
    auto &com = Communication::getInstance(Communication::mspDebugLED);

    uint32_t status = DEFAULT_STATUS_NOTIF;
    uint8_t pingAttempts = 0;
    bool isReadingRawMessage = false;
    for (;;) {
        // TODO change main tasks notifications to queues (race conditions)
        // change status
        xTaskNotifyWait(0, ULONG_MAX, &status, 0);
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
                // TODO remove print
                // Serial.println("vTaskSuspend(mDecodeMessageTaskHandle)");
                vTaskSuspend(com.mDecodeMessageTaskHandle);
                break;
            
            case SUSPEND_ENDCODE_MESSAGE_TASK_NOTIF:
                // TODO remove print
                // Serial.println("vTaskSuspend(mEncodeMessageTaskHandle);");
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

            case STOP_ADDRESING_ALGORITHM_NOTIF:
                isReadingRawMessage = false;
                mspDebugLED->deletePairingBlinkTask();
                com.mpAddressing->stopAddressing();
                break;

            default:
                Serial.print("STATUS ERROR! In communicationMainTask() -> got unknow status. Received Status: ");
                Serial.println(status);
                break;
        }
        // reset notifications status 
        status = DEFAULT_STATUS_NOTIF;
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
        Serial.println("TASK CREATION ERROR! In createCommunicationMainTask() -> Can't create Communication Main task, because task already exists");
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

void Communication::decodeMessageTask(void *parameters) {
    auto &com = Communication::getInstance(Communication::mspDebugLED);

    // timeout status/cause
    uint32_t timeoutStatus = DEFAULT_STATUS_NOTIF;
    // if true decodeMessageTask() will put in xQueueSend() protocolBuffer[0] instead messageBuffer
    bool isRawMessage = false;

    // prepare message protocol buffer
    // [0-5{mac}, 6{ip}, 7{messagesQuantity}, 8-13{message}, 14{checksum}, 15{\0}]
    uint8_t protocolBuffer[PROTOCOL_MESSAGE_MAX_NUM][PROTOCOL_SIZE];

    const uint8_t ipIndex = 6;
    const uint8_t messagesQuantityIndex = 7;
    const uint8_t protocolMessageStartIndex = 8;
    const uint8_t protocolMessageLength = 6;

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
        // TODO add method for handling notifications
        // notifications handling 
        if (xTaskNotifyWait(0, ULONG_MAX, &timeoutStatus, 0) == pdTRUE) {
            if (timeoutStatus == READ_RAW_MESSAGE_NOTIF) {
                isRawMessage = true;
                // TODO remove print
                // Serial.println("Reading raw message");
            } else {
                if (timeoutStatus == BYTE_TIMEOUT_NOTIF) {
                    xTimerStop(com.mReceiveMessageTimeoutTimer, portMAX_DELAY);
                    // TODO remove print
                    Serial.println("Byte timeout");
                    resetProtocolBuffer();
                    xQueueReset(com.mReceiveByteQueue);
                } else if (timeoutStatus == MESSAGE_TIMEOUT_NOTIF) {
                    xTimerStop(com.mReceiveByteTimeoutTimer, portMAX_DELAY);
                    // TODO remove print
                    Serial.println("Message timeout");
                    // TODO change print to concatenate message before using Serial.print
                    for (uint8_t i = 0; i < PROTOCOL_MESSAGE_MAX_NUM; i++){
                        for (uint8_t j = 0; j < PROTOCOL_SIZE; j++) {
                            Serial.print(protocolBuffer[i][j]);
                            Serial.print(' ');
                        }
                    }
                    Serial.println();
                    resetProtocolBuffer();
                    xQueueReset(com.mReceiveByteQueue);
                } else {
                    Serial.print("STATUS ERROR! In decodeMessageTask() -> got unknow status. Received Status: ");
                    Serial.println(timeoutStatus);
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
                    Serial.print("BAD END OF MESSAGE ERROR! In decodeMessageTask() -> message should end with 0 (\\0 char), but got: ");
                    Serial.println(endOfMessage);
                } 
                // if checksum is incorrect
                else if (!com.isCheckSumCorrect(protocolBuffer[protoBuffMessageIndex])) {
                    handleIncorrectMessage(isRawMessage);
                    Serial.println("BAD CHECKSUM ERROR! In decodeMessageTask() -> checksum incorrect");
                }
                // if MAC or IP is incorrect 
                // TODO uncomment and implement
                // else if (!isProperMACAndIP(protocolBuffer[protoBuffMessageIndex], protocolBuffer[protoBuffMessageIndex][6])) {
                else if (false) {
                    // #ifdef CENTRAL_UNIT
                    //     Serial.println("CRITICAL ERROR! In decodeMessageTask() -> isProperMACAndIP() must never return false for CENTRAL_UNIT"); 
                    // #else
                    //     xTimerStop(com.mReceiveMessageTimeoutTimer, portMAX_DELAY);
                    //     resetProtocolBuffer();
                    // #endif
                }
                // if entire message is not ready (message quantity)
                else if (protocolBuffer[protoBuffMessageIndex][messagesQuantityIndex] != 0) {
                    protoBuffMessageIndex++;
                }
                // entire message is ready
                else {
                    xTimerStop(com.mReceiveMessageTimeoutTimer, portMAX_DELAY);

                    // prepare received message buffer
                    uint8_t messageBuffer[MESSAGE_SIZE];
                    uah::clearBuffer(messageBuffer, MESSAGE_SIZE);
                    uint8_t messageIndex = 0;
                    uint8_t messagesQuantity;
                    protoBuffMessageIndex = 0;

                    // decode message
                    // TODO add protection against packet loss (messageQuantity isn't decrementing only by 1) and add separate method for that
                    do {
                        for (uint8_t i = protocolMessageStartIndex; i < (protocolMessageStartIndex + protocolMessageLength); i++) {
                            messageBuffer[messageIndex] = protocolBuffer[protoBuffMessageIndex][i];
                            messageIndex++;
                            // protection against buffer overload (62 => 63 max buffer index -1 for \0)
                            if (messageIndex > MAX_MESSAGE_INDEX - 1) {
                                break;
                            }
                        }
                        messagesQuantity = protocolBuffer[protoBuffMessageIndex][messagesQuantityIndex];
                        protoBuffMessageIndex++;
                    } while(messagesQuantity != 0);

                    if (isRawMessage) {
                        isRawMessage = false;
                        // TODO remove print ?
                        Serial.print("Received raw message: ");
                        uah::printArrayAsInt(protocolBuffer[0], PROTOCOL_SIZE);
                        xQueueSend(com.mReceiveMessageQueue, protocolBuffer[0], portMAX_DELAY);
                    } else {
                        // TODO remove print ?
                        Serial.print("Received message: ");
                        uah::printArrayAsChar(messageBuffer, MESSAGE_SIZE);
                        xQueueSend(com.mReceiveMessageQueue, messageBuffer, portMAX_DELAY);
                    }

                    // clean up
                    resetProtocolBuffer();
                }
            }
        } else {
            xTaskNotify(com.mCommunicationMainTaskHandle, SUSPEND_DECODE_MESSAGE_TASK_NOTIF, eSetValueWithOverwrite);
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
        Serial.println("TASK CREATION ERROR! In createDecodeMessageTask() -> Can't create decode message task, because task already exists");
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
    static constexpr uint8_t checksumIndex = 14;

    protocolBuffer[checksumIndex] = 0;
    uint16_t checkSum = 0;
    for (uint8_t i = 0; i < PROTOCOL_SIZE; i++) {
        checkSum += (uint16_t)protocolBuffer[i];
    }
    checkSum = (CHECKSUM_MODULO - (checkSum % CHECKSUM_MODULO)) % CHECKSUM_MODULO;

    protocolBuffer[checksumIndex] = checkSum;
}

void Communication::encodeMessageTask(void *parameters) {
    auto &com = Communication::getInstance(Communication::mspDebugLED);

    // prepare protocol buffer
    // [0-5{mac}, 6{ip}, 7{messagesQuantity}, 8-13{message}, 14{checksum}, 15{\0}]
    uint8_t protocolBuffer[PROTOCOL_SIZE];
    
    static constexpr uint8_t ipIndex = 6;
    static constexpr uint8_t messagesQuantityIndex = 7;
    static constexpr uint8_t protocolMessageStartIndex = 8;
    static constexpr uint8_t protocolMessageLength = 6;
    
    // TODO implement setting IP address for central unit
    // prepare MAC address in protocol buffer and clear rest of buffer
    uint8_t macAddress[6];
    com.mpAddressing->getProtocolMACAddress(macAddress);
    uah::prepareBuffer(protocolBuffer, macAddress, MAC_ADDRESS_LENGTH, PROTOCOL_SIZE);
    // prepare place for IP address
    protocolBuffer[ipIndex] = com.mpAddressing->getIPAddress();

    // prepare place for message
    for (uint8_t i = protocolMessageStartIndex; i < (protocolMessageStartIndex + protocolMessageLength); i++) {
        protocolBuffer[i] = BLANK_CHARACTER;
    }   

    // prepare message to send buffer
    uint8_t messageBuffer[MESSAGE_SIZE];
    uah::clearBuffer(messageBuffer, MESSAGE_SIZE);

    // task loop
    for (;;) {
        // wait until the message appears in the queue and save message in local messageBuffer
        if (xQueueReceive(com.mSendMessagesQueue, &messageBuffer, pdMS_TO_TICKS(SUSPEND_TASK_TIME_LONG)) == pdTRUE) {
            int8_t messagesQuantity = 0;
            uint8_t messageIndex = 0;
            // calc messageQuantity
            const uint8_t messageLen = uah::calcLenOfDataInArray(messageBuffer, MESSAGE_SIZE);
            messagesQuantity = messageLen / protocolMessageLength;
            if (messageLen % protocolMessageLength == 0) {
                messagesQuantity--;
            }

            // put part of message in protocolBuffer and send it to transmitting task
            for (messagesQuantity; messagesQuantity >= 0; messagesQuantity--) {
                protocolBuffer[messagesQuantityIndex] = messagesQuantity;
                for (uint8_t i = 0; i < protocolMessageLength; i++) {
                    protocolBuffer[protocolMessageStartIndex + i] = messageBuffer[messageIndex] != 0 ? messageBuffer[messageIndex] : BLANK_CHARACTER;
                    messageIndex++;
                }
                // calculate and set checksum
                com.prepareChecksum(protocolBuffer);
                
                com.mpRfModule->addMessageToTransmit(protocolBuffer);

                // TODO remove print 
                // WARNING: long message with uncommented print may cause message timeout
                // Serial.print("protocolBuffer: ");
                // for (int i = 0; i < PROTOCOL_SIZE; i++){
                //     Serial.print(protocolBuffer[i]);
                //     Serial.print(' ');
                // }
                // Serial.println();
                // Serial.print("protocolBuffer message: ");
                // uah::printArrayAsChar(&protocolBuffer[protocolMessageStartIndex], protocolMessageLength);
            }
            if (!uah::areArraysEqual(messageBuffer, (uint8_t*)"repeat", 6)) {
                com.setLastTransmittedMessage(messageBuffer);
            }
        } else {
            xTaskNotify(com.mCommunicationMainTaskHandle, SUSPEND_ENDCODE_MESSAGE_TASK_NOTIF, eSetValueWithOverwrite);
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
        Serial.println("TASK CREATION ERROR! In createEncodeMessageTask() -> Can't create encode message task, because task already exists");
    }
}

void Communication::deleteEncodeMessageTask() {
    if (mEncodeMessageTaskHandle != nullptr) {
        vTaskDelete(mEncodeMessageTaskHandle);
        mEncodeMessageTaskHandle = nullptr;
    }
}
// ================================================================

// TODO consider implementation #ifdef DEBUG_MODE for below section

// ===================== Send Custom Message ======================

void Communication::sendCustomMessageTask(void *parameters) {
    const auto &com = Communication::getInstance(Communication::mspDebugLED);

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
                
                // TODO remove print ?
                Serial.print("Message Ready: ");
                uah::printArrayAsChar(buffer, MESSAGE_SIZE);

                // special debug commands
                if (uah::areArraysEqual(buffer, (uint8_t*)"startping", 9)) {
                    xTaskNotify(com.mCommunicationMainTaskHandle, START_PINGING_NOTIF, eSetValueWithOverwrite);
                } else if (uah::areArraysEqual(buffer, (uint8_t*)"readraw", 7)) {
                    xTaskNotify(com.mCommunicationMainTaskHandle, READ_RAW_MESSAGE_NOTIF, eSetValueWithOverwrite);
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

void Communication::createSendCustomMessageTask() {
    if (mSendCustomMessageTaskHandle == nullptr) {
        xTaskCreate(
            sendCustomMessageTask,
            "Send custom message",
            2048,
            nullptr,
            BACKGROUND_TASK_PRIORITY,
            &mSendCustomMessageTaskHandle
        );
    } else {
        Serial.println("TASK CREATION ERROR! In createSendCustomMessageTask() -> Can't create send custom message task, because task already exists");
    }
}

void Communication::deleteSendCustomMessageTask() {
    if (mSendCustomMessageTaskHandle != nullptr) {
        vTaskDelete(mSendCustomMessageTaskHandle);
        mSendCustomMessageTaskHandle = nullptr;
    }
}
// ================================================================

// ============================ Timers ============================

void Communication::communicationTimersCallbacks(TimerHandle_t xTimer){
    const auto &com = Communication::getInstance(Communication::mspDebugLED);

    if (xTimer == com.mReceiveMessageTimeoutTimer) {
        // TODO remove print
        // Serial.println("message timeout callback");
        xTaskNotify(com.mCommunicationMainTaskHandle, MESSAGE_TIMEOUT_NOTIF, eSetValueWithOverwrite);
    } else if (xTimer == com.mReceiveByteTimeoutTimer) {
        // TODO remove print
        // Serial.println("byte timeout callback");
        xTimerStop(com.mReceiveMessageTimeoutTimer, portMAX_DELAY);
        xTaskNotify(com.mCommunicationMainTaskHandle, BYTE_TIMEOUT_NOTIF, eSetValueWithOverwrite);
    } else if (xTimer == com.mPingTimeoutTimer) {
        xTaskNotify(com.mCommunicationMainTaskHandle, PING_TIMEOUT_NOTIF, eSetValueWithOverwrite);
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
    if (mReceiveMessageTimeoutTimer != nullptr) {
        xTimerDelete(mReceiveMessageTimeoutTimer, portMAX_DELAY);
        mReceiveMessageTimeoutTimer = nullptr;
    }
    if (mReceiveByteTimeoutTimer != nullptr) {
        xTimerDelete(mReceiveByteTimeoutTimer, portMAX_DELAY);
        mReceiveByteTimeoutTimer = nullptr;
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
    mLastTransmittedMessageAttempts = 0;
    xSemaphoreGive(mLastTransmittedMessageMutex);
}

void Communication::transmitRepeatMessage() const {
    uint8_t buffer[MESSAGE_SIZE];
    uah::prepareBuffer(buffer, (uint8_t*)"repeat", 6, MESSAGE_SIZE);
    xQueueSend(mSendMessagesQueue, buffer, portMAX_DELAY);
}

void Communication::repeatLastTransmittedMessage() {
    bool isMaxAttemptsExceeded = false;
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
    uah::prepareBuffer(buffer, (uint8_t*)"ping", 4, MESSAGE_SIZE);
    xQueueSend(mSendMessagesQueue, buffer, portMAX_DELAY);
}

void Communication::replyToPing() const {
    uint8_t buffer[MESSAGE_SIZE];
    uah::prepareBuffer(buffer, (uint8_t*)"reping", 6, MESSAGE_SIZE);
    xQueueSend(mSendMessagesQueue, buffer, portMAX_DELAY);
}
// ================================================================