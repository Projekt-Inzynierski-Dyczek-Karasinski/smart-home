// TODO !BEFORE PULL REQUEST! check for "eSetValueWithoutOverwrite"!
#include "communication/communication.h"

#include <Arduino.h>
#include <HardwareSerial.h>

#include "communication/uint8_array_handlers.h"
#include "universal_module_system/debug_led.h"

#include "smart_home_config.h"
#include "config/communication_config.h"
#include "config/addressing_config.h"

#ifdef HC12_MODULE
    #include "communication/hc12.h"
#else
    #error "Not implemented" 
#endif

#ifdef CENTRAL_UNIT 
    #include "communication/addressing/central_unit_addressing.h"
#else
    #include "communication/addressing/module_addressing.h"
#endif

namespace uah = uint8ArrayHandlers;

DebugLED* Communication::mspDebugLED = nullptr;

// ============================ Public ============================

Communication& Communication::getInstance(DebugLED *debugLED) {
    static Communication instance(debugLED);
    return instance;
}

void Communication::startAddresingAlgorithm() {
    mspDebugLED->createPairingBlinkTask();
    mpAddressing->startAddressing();
}

void Communication::stopAddresingAlgorithm() {
    xTaskNotify(mCommunicationMainTaskHandle, stopAddresingAlgorithmNotif, eSetValueWithOverwrite);
}

void Communication::needRawMessage() {
    xTaskNotify(mCommunicationMainTaskHandle, readRawMessageNotif, eSetValueWithOverwrite);
}

void Communication::resetEncodeMessageTask() {
    taskENTER_CRITICAL(&mCriticalSectionMutex);
    deleteEncodeMessageTask();
    createEncodeMessageTask();
    taskEXIT_CRITICAL(&mCriticalSectionMutex);
}

void Communication::startPinging() {
    xTaskNotify(mCommunicationMainTaskHandle, startPingingNotif, eSetValueWithOverwrite);
}

void Communication::addByteToDecode(const uint8_t DATA) {
    xQueueSend(mReceiveByteQueue, &DATA, portMAX_DELAY);
    vTaskResume(mDecodeMessageTaskHandle);
}

void Communication::sendMessage(const uint8_t MESSAGE[MESSAGE_SIZE]) {
    xQueueSend(mSendMessagesQueue, MESSAGE, portMAX_DELAY);
    vTaskResume(mEncodeMessageTaskHandle);
} 

void Communication::sendInternalMessage(const uint8_t MESSAGE[MESSAGE_SIZE]) {
    xQueueSend(mReceiveMessageQueue, MESSAGE, portMAX_DELAY);
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
    if (mReceiveMessageQueue == NULL) {
        mReceiveMessageQueue = xQueueCreate(MESSAGE_QUEUE_LEN, sizeof(uint8_t[MESSAGE_SIZE]));
    }
    if (mReceiveByteQueue == NULL) {
        mReceiveByteQueue = xQueueCreate(RECEIVE_BYTE_QUEUE_LEN, sizeof(uint8_t));
    }
    if (mSendMessagesQueue == NULL) {
        mSendMessagesQueue = xQueueCreate(MESSAGE_QUEUE_LEN, sizeof(uint8_t[MESSAGE_SIZE]));
    }
}

void Communication::deleteCommunicationQueues() {
    if (mReceiveMessageQueue != NULL) {
        vQueueDelete(mReceiveMessageQueue);
        mReceiveMessageQueue = NULL;
    }
    if (mReceiveByteQueue != NULL) {
        vQueueDelete(mReceiveByteQueue);
        mReceiveByteQueue = NULL;
    }
    if (mSendMessagesQueue != NULL) {
        vQueueDelete(mSendMessagesQueue);
        mSendMessagesQueue = NULL;
    }
}
// ================================================================

// ====================== Communication Main ======================

void Communication::receivedMessageDecider(bool *isReadingRawMessage) {
    uint8_t buffor[MESSAGE_SIZE];
    if (xQueueReceive(mReceiveMessageQueue, buffor, 0) == pdTRUE) {
        // if it is "repeat" message repeat last transmitted message
        if (uah::areArraysEqual(buffor, (uint8_t*)"repeat", 6)) {
            repeatLastTransmittedMessage();
        } 
        // if it is "ping", reply to ping
        else if (uah::areArraysEqual(buffor, (uint8_t*)"ping", 4)) {
            replyToPing();
        }
        // if it is "reping", reply to ping
        else if (uah::areArraysEqual(buffor, (uint8_t*)"reping", 6)) {
            xTimerStop(mPingTimeoutTimer, portMAX_DELAY);
            Serial.println("Ping Success");
        }
        // if is addressing message
        else if ((buffor[0] == (uint8_t)'A' && buffor[1] == (uint8_t)'D') || *isReadingRawMessage) {
            *isReadingRawMessage = false;
            mpAddressing->addMessage(buffor);
        }
        // if is HC12 command
        #ifdef HC12_MODULE
        else if (buffor[0] == (uint8_t)'H' && buffor[1] == (uint8_t)'C') {
            mpRfModule->setupHC12(buffor);
        }
        #endif
        else {
            // TODO uncomment
            // Serial.println("COMMUNICATION WARNING! In receivedMessageDecider() -> decider doesn't know what to do with received message");
            // Serial.print("Ignored message: ");
            uah::printArrayAsChar(buffor, MESSAGE_SIZE);
        }
        *isReadingRawMessage = false;
    }
}

void Communication::communicationMainTask(void* parameters) {
    auto &com = Communication::getInstance(Communication::mspDebugLED);

    uint32_t status = defaultStatusNotif;

    uint8_t pingAttempts = 0;
    bool isReadingRawMessage = false;

    for (;;) {
        // TODO !BEFORE PULL REQUEST! change there and in other main tasks notifications to queues (race conditions)
        // change status
        xTaskNotifyWait(0, ULONG_MAX, &status, 0);
        switch (status) {
            case defaultStatusNotif:
                // if queue is not empty resume corresponding task
                if (uxQueueMessagesWaiting(com.mReceiveByteQueue) != 0) {
                    vTaskResume(com.mDecodeMessageTaskHandle);
                }
                if (uxQueueMessagesWaiting(com.mSendMessagesQueue) != 0) {
                    vTaskResume(com.mEncodeMessageTaskHandle);
                }

                // monitor incoming rf messages
                com.receivedMessageDecider(&isReadingRawMessage);

                // delay for watchdog
                vTaskDelay(pdMS_TO_TICKS(1));
                break;
                
            case byteTimeoutNotif:
                xTaskNotify(com.mDecodeMessageTaskHandle, byteTimeoutNotif, eSetValueWithOverwrite);
                break;

            case messageTimeoutNotif:
                xTaskNotify(com.mDecodeMessageTaskHandle, messageTimeoutNotif, eSetValueWithOverwrite);
                break;

            case suspendDecodeMessageTaskNotif:
                // TODO remove print
                // Serial.println("vTaskSuspend(mDecodeMessageTaskHandle)");
                vTaskSuspend(com.mDecodeMessageTaskHandle);
                break;
            
            case suspendEndcodeMessageTaskNotif:
                // TODO remove print
                // Serial.println("vTaskSuspend(mEncodeMessageTaskHandle);");
                com.setLastTransmittedMessage();
                vTaskSuspend(com.mEncodeMessageTaskHandle);
                break;  
            
            case startPingingNotif:
                pingAttempts = 1;
                com.transmitPing();
                xTimerStart(com.mPingTimeoutTimer, portMAX_DELAY);
                break;

            case sendPingNotif:
                Serial.println("Ping Timeout");
                pingAttempts++;

                if (pingAttempts > PING_MAX_ATTEMPTS) {
                    xTimerStop(com.mPingTimeoutTimer, portMAX_DELAY);
                    pingAttempts = 0;
                    Serial.println("PING ERROR! In communicationMainTask() -> no response to ping.");
                } else {
                    com.transmitPing();
                }
                break;

            case readRawMessageNotif:
                isReadingRawMessage = true;
                xTaskNotify(com.mDecodeMessageTaskHandle, readRawMessageNotif, eSetValueWithOverwrite);
                break;

            case stopAddresingAlgorithmNotif:
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
        status = defaultStatusNotif;
    }
}

void Communication::createCommunicationMainTask() {
    if (mCommunicationMainTaskHandle == NULL) {
        xTaskCreate(
            communicationMainTask,
            "Communication Main",
            2048,
            NULL,
            BACKGROUND_TASK_PRIORITY,
            &mCommunicationMainTaskHandle
        );
    } else {
        Serial.println("TASK CREATION ERROR! In createCommunicationMainTask() -> Can't create Communication Main task, because task already exists");
    }
}
void Communication::deleteCommunicationMainTask() {
    if (mCommunicationMainTaskHandle != NULL) {
        vTaskDelete(mCommunicationMainTaskHandle);
        mCommunicationMainTaskHandle = NULL;
    }
}
// ================================================================

// ======================== Decode Message ========================

void Communication::decodeMessageTask(void *parameters) {
    auto &com = Communication::getInstance(Communication::mspDebugLED);

    // timeout status/cause
    uint32_t timeoutStatus = defaultStatusNotif;
    // if true decodeMessageTask() will put in xQueueSend() protocolBuffor[0] insted messageBuffor
    bool isRawMessage = false;

    // prepare message protocol buffor
    // [0-5{mac}, 6{ip}, 7{messagesQuantity}, 8-13{message}, 14{checksum}, 15{\0}]
    uint8_t protocolBuffor[PROTOCOL_MESSAGE_MAX_NUM][PROTOCOL_SIZE];
    const uint8_t IP_INDEX = 6;
    const uint8_t MESSAGES_QUANTITY_INDEX = 7;
    const uint8_t PROTOCOL_MESSAGE_START_INDEX = 8;
    const uint8_t PROTOCOL_MESSAGE_LENGTH = 6;
    uint8_t pbMessageIndex = 0;
    uint8_t pbByteIndex = 0;

    auto resetProtocolBuffor = [&]() {
        for (uint8_t i = 0; i < PROTOCOL_MESSAGE_MAX_NUM; i++){
            for (uint8_t j = 0; j < PROTOCOL_SIZE; j++) {
                protocolBuffor[i][j] = 0;
            }
        }
        pbByteIndex = 0;
        pbMessageIndex = 0;
    };
    resetProtocolBuffor();

    // buffor for byte received from rfModule
    uint8_t queueBuffor;

    // task loop
    for (;;) {
        // notifications handling 
        if (xTaskNotifyWait(0, ULONG_MAX, &timeoutStatus, 0) == pdTRUE) {
            if (timeoutStatus == readRawMessageNotif) {
                isRawMessage = true;
                // TODO remove print
                Serial.println("isRawMessage = true;");
            } else {
                if (timeoutStatus == byteTimeoutNotif) {
                    xTimerStop(com.mReceiveMessageTimeoutTimer, portMAX_DELAY);
                    // TODO remove print
                    Serial.println("Byte timeout");
                    resetProtocolBuffor();
                    xQueueReset(com.mReceiveByteQueue);
                } else if (timeoutStatus == messageTimeoutNotif) {
                    xTimerStop(com.mReceiveByteTimeoutTimer, portMAX_DELAY);
                    // TODO remove print
                    Serial.println("Message timeout");
                    for (uint8_t i = 0; i < PROTOCOL_MESSAGE_MAX_NUM; i++){
                        for (uint8_t j = 0; j < PROTOCOL_SIZE; j++) {
                            Serial.print(protocolBuffor[i][j]);
                            Serial.print(' ');
                        }
                    }
                    Serial.println();
                    resetProtocolBuffor();
                    xQueueReset(com.mReceiveByteQueue);
                } else {
                    Serial.print("STATUS ERROR! In decodeMessageTask() -> got unknow status. Received Status: ");
                    Serial.println(timeoutStatus);
                }
                isRawMessage = false;
            }
            timeoutStatus = defaultStatusNotif;
        } 

        // decoding message
        if (xQueueReceive(com.mReceiveByteQueue, &queueBuffor, pdMS_TO_TICKS(SUSPEND_TASK_TIME_LONG)) == pdTRUE) {
            protocolBuffor[pbMessageIndex][pbByteIndex] = queueBuffor;
            // if new message start message timeout timer
            if (pbByteIndex == 0 && pbMessageIndex == 0){
                xTimerStart(com.mReceiveMessageTimeoutTimer, portMAX_DELAY);
            }
            // if protocol message is not complete
            if (pbByteIndex != PROTOCOL_SIZE - 1) {
                pbByteIndex++;
                xTimerStart(com.mReceiveByteTimeoutTimer, portMAX_DELAY);
            } else {
                xTimerStop(com.mReceiveByteTimeoutTimer, portMAX_DELAY);
                pbByteIndex = 0;

                // if message is not end properly
                if (protocolBuffor[pbMessageIndex][PROTOCOL_SIZE - 1] != 0) {
                    xTimerStop(com.mReceiveMessageTimeoutTimer, portMAX_DELAY);
                    
                    Serial.print("BAD END OF MESSAGE ERROR! In decodeMessageTask() -> message should end with 0 (\\0 char), but got: ");
                    Serial.println(protocolBuffor[pbMessageIndex][PROTOCOL_SIZE - 1]);

                    // TODO assign final value
                    // wait for possible rest of the message and send "repeat"
                    vTaskDelay(RECEIVE_MESSAGE_TIMEOUT);
                    resetProtocolBuffor();
                    xQueueReset(com.mReceiveByteQueue);
                    if (!isRawMessage) {
                        com.transmitRepeatMessage();                        
                    }
                } else {
                    // calculate checksum
                    uint16_t checksum = 0;
                    for (uint8_t i = 0; i < PROTOCOL_SIZE; i++) {
                        checksum += protocolBuffor[pbMessageIndex][i];
                    }
                    
                    // if checksum is incorrect
                    if (checksum % 256 != 0) {
                        xTimerStop(com.mReceiveMessageTimeoutTimer, portMAX_DELAY);
                        Serial.println("BAD CHECKSUM ERROR! In decodeMessageTask() -> checksum incorrect");

                        // TODO assign final value
                        // wait for possible rest of the message and send "repeat"
                        vTaskDelay(RECEIVE_MESSAGE_TIMEOUT);
                        resetProtocolBuffor();
                        xQueueReset(com.mReceiveByteQueue);
                        if (!isRawMessage) {
                            com.transmitRepeatMessage();                            
                        }
                    }
                    // if MAC or IP is incorrect 
                    // TODO uncomment and implement
                    // else if (!isProperMACAndIP(protocolBuffor[pbMessageIndex], protocolBuffor[pbMessageIndex][6])) {
                    else if (false) {
                        #ifdef CENTRAL_UNIT
                            Serial.println("CRITICAL ERROR! In decodeMessageTask() -> isProperMACAndIP() must never return false for CENTRAL_UNIT"); 
                        #else
                            xTimerStop(com.mReceiveMessageTimeoutTimer, portMAX_DELAY);
                            resetProtocolBuffor();
                        #endif
                    }
                    // if entire message is not ready (message quantity)
                    else if (protocolBuffor[pbMessageIndex][MESSAGES_QUANTITY_INDEX] != 0) {
                        pbMessageIndex++;
                    }
                    // entire message is ready
                    else {
                        xTimerStop(com.mReceiveMessageTimeoutTimer, portMAX_DELAY);

                        // prepare received message buffor
                        uint8_t messageBuffor[MESSAGE_SIZE];
                        uah::prepareBuffor(messageBuffor, MESSAGE_SIZE);
                        uint8_t messageIndex = 0;
                        uint8_t messagesQuantity;
                        pbMessageIndex = 0;

                        // decode message
                        do {
                            for (uint8_t i = PROTOCOL_MESSAGE_START_INDEX; i < (PROTOCOL_MESSAGE_START_INDEX + PROTOCOL_MESSAGE_LENGTH); i++) {
                                messageBuffor[messageIndex] = protocolBuffor[pbMessageIndex][i];
                                messageIndex++;
                                // protection against buffer overload (62 => 63 max buffer index -1 for \0)
                                if (messageIndex > 62) {
                                    // TODO add what to do with longer messages 
                                    break;
                                }
                            }

                            messagesQuantity = protocolBuffor[pbMessageIndex][MESSAGES_QUANTITY_INDEX];
                            pbMessageIndex++;
                        } while(messagesQuantity != 0);

                        // TODO remove print ?
                        Serial.print("Received message: ");
                        uah::printArrayAsChar(messageBuffor, MESSAGE_SIZE);
                        // TODO remove print 
                        // uah::printArrayAsInt(protocolBuffor[0], PROTOCOL_SIZE);

                        if (isRawMessage) {
                            isRawMessage = false;
                            xQueueSend(com.mReceiveMessageQueue, protocolBuffor[0], portMAX_DELAY);
                        } else {
                            xQueueSend(com.mReceiveMessageQueue, messageBuffor, portMAX_DELAY);
                        }

                        // TODO remove
                        // // if it is "repeat" message
                        // if (uah::areArraysEqual(messageBuffor, (uint8_t*)"repeat", 6)) {
                        //     com.repeatLastTransmittedMessage();
                        // } 
                        // #ifdef HC12_MODULE
                        // // if it is HC_12 command
                        // else if (messageBuffor[0] == (uint8_t)'H' && messageBuffor[1] == (uint8_t)'C') {
                        //     com.mpRfModule->setupHC12(messageBuffor);
                        // }
                        // #endif
                        // TODO implement
                        // if Addressing Task is working
                        // else if (msAddressingTaskHandle != NULL) {
                        //     if (isRawMessage) {
                        //         xQueueSend(msReceiveMessageQueue, protocolBuffor[0], portMAX_DELAY);
                        //         isRawMessage = false;
                        //     } else {
                        //         xQueueSend(msReceiveMessageQueue, messageBuffor, portMAX_DELAY);
                        //     }
                        // }
                        // TODO add message to queue
                        // else

                        // clean up
                        resetProtocolBuffor();
                    }
                }
            }
        } else {
            xTaskNotify(com.mCommunicationMainTaskHandle, suspendDecodeMessageTaskNotif, eSetValueWithOverwrite);
        }
    }
}

void Communication::createDecodeMessageTask() {
    if (mDecodeMessageTaskHandle == NULL) {
        xTaskCreate(
            decodeMessageTask,
            "Decode message",
            2048,
            NULL,
            LOW_TASK_PRIORITY,
            &mDecodeMessageTaskHandle
        );
    } else {
        Serial.println("TASK CREATION ERROR! In createDecodeMessageTask() -> Can't create decode message task, because task already exists");
    }
}
void Communication::deleteDecodeMessageTask() {
    if (mDecodeMessageTaskHandle != NULL) {
        vTaskDelete(mDecodeMessageTaskHandle);
        mDecodeMessageTaskHandle = NULL;
    }
}
// ================================================================

// ======================== Encode Message ========================

void Communication::encodeMessageTask(void *parameters) {
    auto &com = Communication::getInstance(Communication::mspDebugLED);

    // prepare protocol buffor
    // [0-5{mac}, 6{ip}, 7{messagesQuantity}, 8-13{message}, 14{checksum}, 15{\0}]
    uint8_t protocolBuffor[PROTOCOL_SIZE];
    
    static constexpr uint8_t IP_INDEX = 6;
    static constexpr uint8_t MESSAGES_QUANTITY_INDEX = 7;
    static constexpr uint8_t PROTOCOL_MESSAGE_START_INDEX = 8;
    static constexpr uint8_t PROTOCOL_MESSAGE_LENGTH = 6;
    static constexpr uint8_t CHECKSUM_INDEX = 14;

    // prepare MAC address in protocol buffor and clear rest of buffor
    uint8_t macAddress[6];
    com.mpAddressing->getProtocolMACAddress(macAddress);
    uah::prepareBuffor(protocolBuffor, macAddress, MAC_ADDRESS_LENGTH, PROTOCOL_SIZE);
    // prepare place for IP address
    protocolBuffor[IP_INDEX] = com.mpAddressing->getIPAddress();

    // TODO remove old
    // prepare IP and MAC addresses
    // #ifdef CENTRAL_UNIT
    //     for (uint8_t j = 0; j < 6; j++){
    //         protocolBuffor[i][j] = msMACAddress[j];
    //     }
    //     protocolBuffor[i][6] = 1;
    // #else
    //     xSemaphoreTake(msAddressDataMutex, portMAX_DELAY);
    //     protocolBuffor[i][6] = msIPAddress;
    //     if (msIPAddress == 0) {
    //         for (uint8_t j = 0; j < 6; j++){
    //             protocolBuffor[i][j] = msMACAddress[j];
    //         }
    //     } else {
    //         for (uint8_t j = 0; j < 6; j++){
    //             protocolBuffor[i][j] = msCentralUnitMACAddress[j];
    //         }
    //     }
    //     xSemaphoreGive(msAddressDataMutex);
    // #endif
    
    // prepare place for message
    for (uint8_t i = PROTOCOL_MESSAGE_START_INDEX; i < (PROTOCOL_MESSAGE_START_INDEX + PROTOCOL_MESSAGE_LENGTH); i++) {
        protocolBuffor[i] = BLANK_CHARACTER;
    }   

    // prepare message to send buffor
    uint8_t messageBuffor[MESSAGE_SIZE];
    uah::prepareBuffor(messageBuffor, MESSAGE_SIZE);

    // task loop
    for (;;) {
        uint8_t messageIndex = 0;
        int8_t messagesQuantity = 0;

        // wait until the message appears in the queue and save message in local messageBuffor
        if (xQueueReceive(com.mSendMessagesQueue, &messageBuffor, pdMS_TO_TICKS(SUSPEND_TASK_TIME_LONG)) == pdTRUE) {
            // calc messageQuantity
            uint8_t messageLen = uah::calcLenOfDataInArray(messageBuffor, MESSAGE_SIZE);
            messagesQuantity = messageLen / 6;
            if (messageLen % 6 == 0) {
                messagesQuantity--;
            }

            // put part of message in protocolBuffor and send it to transmiting task 
            for (messagesQuantity; messagesQuantity >= 0; messagesQuantity--) {
                protocolBuffor[MESSAGES_QUANTITY_INDEX] = messagesQuantity;
                for (uint8_t i = 0; i < PROTOCOL_MESSAGE_LENGTH; i++) {
                    protocolBuffor[PROTOCOL_MESSAGE_START_INDEX + i] = messageBuffor[messageIndex] != 0 ? messageBuffor[messageIndex] : BLANK_CHARACTER;
                    messageIndex++;
                }

                // calculate and set checksum
                protocolBuffor[CHECKSUM_INDEX] = 0;
                uint16_t checkSum = 0;
                for (uint8_t i = 0; i < PROTOCOL_SIZE; i++) {
                    checkSum += (uint16_t)protocolBuffor[i];
                }
                checkSum = (256 - (checkSum % 256)) % 256;
                protocolBuffor[CHECKSUM_INDEX] = checkSum;
                
                com.mpRfModule->addMessageToTransmit(protocolBuffor);

                // TODO remove print 
                // WARNING: long message with uncommented print may cause message timeout
                // Serial.print("protocolBuffor: ");
                // for (int i = 0; i < PROTOCOL_SIZE; i++){
                //     Serial.print(protocolBuffor[i]);
                //     Serial.print(' ');
                // }
                // Serial.println();
                // Serial.print("protocolBuffor message: ");
                // uah::printArrayAsChar(&protocolBuffor[PROTOCOL_MESSAGE_START_INDEX], PROTOCOL_MESSAGE_LENGTH);
            }

            if (!uah::areArraysEqual(messageBuffor, (uint8_t*)"repeat", 6)) {
                com.setLastTransmittedMessage(messageBuffor);
            }
        } else {
            xTaskNotify(com.mCommunicationMainTaskHandle, suspendEndcodeMessageTaskNotif, eSetValueWithOverwrite);
        }
    }
}

void Communication::createEncodeMessageTask() {
    if (mEncodeMessageTaskHandle == NULL) {
        xTaskCreate(
            encodeMessageTask,
            "Encode Message",
            2048,
            NULL,
            MEDIUM_TASK_PRIORITY,
            &mEncodeMessageTaskHandle
        );
    } else {
        Serial.println("TASK CREATION ERROR! In createEncodeMessageTask() -> Can't create encode message task, because task already exists");
    }
}

void Communication::deleteEncodeMessageTask() {
    if (mEncodeMessageTaskHandle != NULL) {
        vTaskDelete(mEncodeMessageTaskHandle);
        mEncodeMessageTaskHandle = NULL;
    }
}
// ================================================================

// TODO consider implementation #ifdef DEBUG_MODE for below section

// ===================== Send Custom Message ======================

void Communication::sendCustomMessageTask(void *parameters) {
    auto &com = Communication::getInstance(Communication::mspDebugLED);

    // prepare buffor
    uint8_t buffor[MESSAGE_SIZE];
    for (uint8_t i = 0; i < MESSAGE_SIZE; i++){
        buffor[i] = 0;
    }
    uint8_t index = 0;

    // task loop
    for(;;) {
        if (Serial.available() > 0) {
            buffor[index] = Serial.read();
            if (buffor[index] != 13) {
                index++;
            }

            // send message to Prepare Message To Send (protocol)
            if (buffor[index - 1] == (uint8_t)'\n') {
                buffor[index - 1] = 0;
                
                // TODO remove debug print
                Serial.print("Message Ready: ");
                uah::printArrayAsChar(buffor, MESSAGE_SIZE);

                // special debug commands
                if (uah::areArraysEqual(buffor, (uint8_t*)"startping", 9)) {
                    xTaskNotify(com.mCommunicationMainTaskHandle, startPingingNotif, eSetValueWithOverwrite);
                } else if (uah::areArraysEqual(buffor, (uint8_t*)"readraw", 7)) {
                    xTaskNotify(com.mCommunicationMainTaskHandle, readRawMessageNotif, eSetValueWithOverwrite);
                } 
                // rest
                else {
                    // check if is HC_12 command
                    #ifdef HC12_MODULE
                        if (buffor[0] == 'A' && buffor[1] == 'T') {
                            buffor[0] = 'H';
                            buffor[1] = 'C';
                            xQueueSend(com.mReceiveMessageQueue, &buffor, portMAX_DELAY);
                        } else {
                            xQueueSend(com.mSendMessagesQueue, &buffor, portMAX_DELAY);
                        }
                    #else
                        #error "not implemented"
                    #endif
                }

                // reset buffor
                uah::prepareBuffor(buffor, index);
                index = 0;
            }
        }
        // delay for watchdog
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void Communication::createSendCustomMessageTask() {
    if (mSendCustomMessageTaskHandle == NULL) {
        xTaskCreate(
            sendCustomMessageTask,
            "Send custom message",
            2048,
            NULL,
            BACKGROUND_TASK_PRIORITY,
            &mSendCustomMessageTaskHandle
        );
    } else {
        Serial.println("TASK CREATION ERROR! In createSendCustomMessageTask() -> Can't create send custom message task, because task already exists");
    }
}
void Communication::deleteSendCustomMessageTask() {
    if (mSendCustomMessageTaskHandle != NULL) {
        vTaskDelete(mSendCustomMessageTaskHandle);
        mSendCustomMessageTaskHandle = NULL;
    }
}
// ================================================================

// ============================ Timers ============================

void Communication::communicationTimersCallbacks(TimerHandle_t xTimer){
    auto &com = Communication::getInstance(Communication::mspDebugLED);

    if (xTimer == com.mReceiveMessageTimeoutTimer) {
        // TODO remove print
        Serial.println("message timeout callback");
        xTaskNotify(com.mCommunicationMainTaskHandle, messageTimeoutNotif, eSetValueWithOverwrite);
    } else if (xTimer == com.mReceiveByteTimeoutTimer) {
        // TODO remove print
        Serial.println("byte timeout callback");
        xTimerStop(com.mReceiveMessageTimeoutTimer, portMAX_DELAY);
        xTaskNotify(com.mCommunicationMainTaskHandle, byteTimeoutNotif, eSetValueWithOverwrite);
    } else if (xTimer == com.mPingTimeoutTimer) {
        xTaskNotify(com.mCommunicationMainTaskHandle, sendPingNotif, eSetValueWithOverwrite);
    }
}

void Communication::createCommunicationTimers() {
    if (mReceiveMessageTimeoutTimer == NULL) {
        mReceiveMessageTimeoutTimer = xTimerCreate(
            "Receive Message Timeout",
            pdMS_TO_TICKS(RECEIVE_MESSAGE_TIMEOUT),
            pdFALSE,
            NULL,
            communicationTimersCallbacks
        );
    }
    if (mReceiveByteTimeoutTimer == NULL) {
        mReceiveByteTimeoutTimer = xTimerCreate(
            "Receive Byte Timeout",
            pdMS_TO_TICKS(RECEIVE_BYTE_TIMEOUT),
            pdFALSE,
            NULL,
            communicationTimersCallbacks
        );
    }
    if (mPingTimeoutTimer == NULL) {
        mPingTimeoutTimer = xTimerCreate(
            "Ping Timeout",
            pdMS_TO_TICKS(RECEIVE_MESSAGE_TIMEOUT),
            pdTRUE,
            NULL,
            communicationTimersCallbacks
        );
    }
}

void Communication::deleteCommunicationTimers() {
    if (mReceiveMessageTimeoutTimer != NULL) {
        xTimerDelete(mReceiveMessageTimeoutTimer, portMAX_DELAY);
        mReceiveMessageTimeoutTimer = NULL;
    }
    if (mReceiveByteTimeoutTimer != NULL) {
        xTimerDelete(mReceiveByteTimeoutTimer, portMAX_DELAY);
        mReceiveByteTimeoutTimer = NULL;
    }
}
// ================================================================

// ============================ Other =============================

void Communication::setLastTransmittedMessage() {
    xSemaphoreTake(mLastTransmittedMessageMutex, portMAX_DELAY);
    uah::prepareBuffor(mLastTransmittedMessage, MESSAGE_SIZE);
    mLastTransmittedMessageAttempts = 0;
    xSemaphoreGive(mLastTransmittedMessageMutex);
}

void Communication::setLastTransmittedMessage(const uint8_t MESSAGE[MESSAGE_SIZE]) {
    xSemaphoreTake(mLastTransmittedMessageMutex, portMAX_DELAY);
    uah::prepareBuffor(mLastTransmittedMessage, MESSAGE, MESSAGE_SIZE, MESSAGE_SIZE);
    mLastTransmittedMessageAttempts = 0;
    xSemaphoreGive(mLastTransmittedMessageMutex);
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

void Communication::transmitRepeatMessage() {
    uint8_t buffor[MESSAGE_SIZE];
    uah::prepareBuffor(buffor, (uint8_t*)"repeat", 6, MESSAGE_SIZE);
    xQueueSend(mSendMessagesQueue, buffor, portMAX_DELAY);
}

void Communication::transmitPing() {
    uint8_t buffor[MESSAGE_SIZE];
    uah::prepareBuffor(buffor, (uint8_t*)"ping", 4, MESSAGE_SIZE);
    xQueueSend(mSendMessagesQueue, buffor, portMAX_DELAY);
}

void Communication::replyToPing() {
    uint8_t buffor[MESSAGE_SIZE];
    uah::prepareBuffor(buffor, (uint8_t*)"reping", 6, MESSAGE_SIZE);
    xQueueSend(mSendMessagesQueue, buffor, portMAX_DELAY);
}
// ================================================================