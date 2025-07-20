// TODO !BEFORE PULL REQUEST! check for "eSetValueWithoutOverwrite"!
// TODO !BEFORE PULL REQUEST! change setting buffors "by hand" to using method prepareMessageBuffor()
#include "communication/communication.h"

#include <Arduino.h>
#include <HardwareSerial.h>

#include "smart_home_config.h"
#include "config/communication_config.h"
#include "communication/uint8_array_handlers.h"
#include "universal_module_system/debug_led.h"
#ifdef HC12_MODULE
    #include "communication/hc12.h"
#else
    #error "Not implemented" 
#endif

namespace uah = uint8ArrayHandlers;

DebugLED* Communication::mspDebugLED = nullptr;

// ============================ Public ============================

Communication& Communication::getInstance(DebugLED *debugLED) {
    static Communication instance(debugLED);
    return instance;
}

// TODO implement
void Communication::startAddresingAlgorithm() {
    Serial.println("startAddresingAlgorithm() not implemented");
}

void Communication::addByteToDecode(const uint8_t DATA) {
    xQueueSend(mReceiveByteQueue, &DATA, portMAX_DELAY);
    vTaskResume(mReceiveMessageTaskHandle);
}
// ================================================================

// ================== Constructor and Destructor ==================

Communication::Communication(DebugLED *debugLED) : 
    #ifdef HC12_MODULE
        mRfModule(new HC12(this)) 
    #else
        #error "Not implemented" 
    #endif
{
    mspDebugLED = debugLED;
    #ifdef ESP32_BOARD
        esp_read_mac(mMACAddress, ESP_MAC_WIFI_STA);
        mIsMacAddressReal = true;
    #else
        // TODO add function to get MAC address on different boards
        #error "MAC address not implemented!"
    #endif

    createCommunicationQueues();
    createCommunicationTimers();
    
    createSendCustomMessageTask();
    createReceiveMessageTask();
    createSendMessageTask();
    createCommunicationMainTask();

    Serial.println("Communication initialized"); 
}

Communication::~Communication() {
    deleteSendCustomMessageTask();
    deleteSendMessageTask();
    deleteReceiveMessageTask();
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
        // TODO assign propper length of queue
        // TODO remove this "magic number" 
        mReceiveByteQueue = xQueueCreate(128, sizeof(uint8_t));
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

void Communication::communicationMainTask(void* parameters) {
    auto &com = Communication::getInstance(Communication::mspDebugLED);

    uint32_t status = defaultStatusNotif;

    bool isSendingTaskWaiting = false;
    
    for (;;) {
        // change status
        xTaskNotifyWait(0, ULONG_MAX, &status, 0);
        switch (status) {
            case defaultStatusNotif:
                // if queue is not empty resume corresponding task
                if (uxQueueMessagesWaiting(com.mReceiveByteQueue) != 0) {
                    // TODO remove print
                    Serial.println("vTaskResume(msReceiveMessageTaskHandle);");
                    vTaskResume(com.mReceiveMessageTaskHandle);
                }
                if (uxQueueMessagesWaiting(com.mSendMessagesQueue) != 0) {
                    // TODO remove print
                    Serial.println("vTaskResume(msSendMessageTaskHandle);");
                    vTaskResume(com.mSendMessageTaskHandle);
                }

                // delay for watchdog
                vTaskDelay(pdMS_TO_TICKS(1));
                break;
                
            case byteTimeoutNotif:
                xTaskNotify(com.mReceiveMessageTaskHandle, byteTimeoutNotif, eSetValueWithOverwrite);
                break;

            case messageTimeoutNotif:
                xTaskNotify(com.mReceiveMessageTaskHandle, messageTimeoutNotif, eSetValueWithOverwrite);
                break;

            // case readRawMessageNotif:
            //     // TODO implement
            //     xTaskNotify(msReceiveMessageTaskHandle, readRawMessageNotif, eSetValueWithOverwrite);
            //     break;

            case suspendReceiveMessageTaskNotif:
                // TODO remove print
                Serial.println("vTaskSuspend(msReceiveMessageTaskHandle)");
                vTaskSuspend(com.mReceiveMessageTaskHandle);
                break;
            
            case suspendSendMessageTaskNotif:
                // TODO remove print
                Serial.println("vTaskSuspend(msSendMessageTaskHandle);");
                // TODO implement
                // resetLastMessage();
                vTaskSuspend(com.mSendMessageTaskHandle);
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

// ======================= Receive Message ========================

void Communication::receiveMessageTask(void *parameters) {
    auto &com = Communication::getInstance(Communication::mspDebugLED);

    // timeout status/cause
    uint32_t timeoutStatus = defaultStatusNotif;
    // if true receiveMessageTask() will put in xQueueSend() protocolBuffor[0] insted messageBuffor
    bool isRawMessage = false;

    // prepare message protocol buffor
    // [0-5{mac}, 6{ip}, 7{messagesQuantity}, 8-13{message}, 14{checksum}, 15{\0}]
    uint8_t protocolBuffor[PROTOCOL_MESSAGE_MAX_NUM][PROTOCOL_MESSAGE_SIZE];
    uint8_t pbMessageIndex = 0;
    uint8_t pbByteIndex = 0;

    auto resetProtocolBuffor = [&]() {
        for (uint8_t i = 0; i < PROTOCOL_MESSAGE_MAX_NUM; i++){
            for (uint8_t j = 0; j < PROTOCOL_MESSAGE_SIZE; j++) {
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
                        for (uint8_t j = 0; j < PROTOCOL_MESSAGE_SIZE; j++) {
                            Serial.print(protocolBuffor[i][j]);
                            Serial.print(' ');
                        }
                    }
                    Serial.println();
                    resetProtocolBuffor();
                    xQueueReset(com.mReceiveByteQueue);
                } else {
                    Serial.print("STATUS ERROR! In receiveMessageTask() -> got unknow status. Received Status: ");
                    Serial.println(timeoutStatus);
                }
            }
            timeoutStatus = defaultStatusNotif;
        } 

        // decoding message
        if (xQueueReceive(com.mReceiveByteQueue, &queueBuffor, pdMS_TO_TICKS(RECEIVE_BYTE_TIMEOUT)) == pdTRUE) {
            xTimerStop(com.mSuspendReceiveMessageTimer, portMAX_DELAY);
            protocolBuffor[pbMessageIndex][pbByteIndex] = queueBuffor;
            // if new message start message timeout timer
            if (pbByteIndex == 0 && pbMessageIndex == 0){
                xTimerStart(com.mReceiveMessageTimeoutTimer, portMAX_DELAY);
            }
            // if protocol message is not complete
            if (pbByteIndex != PROTOCOL_MESSAGE_SIZE - 1) {
                pbByteIndex++;
                xTimerStart(com.mReceiveByteTimeoutTimer, portMAX_DELAY);
            } else {
                xTimerStop(com.mReceiveByteTimeoutTimer, portMAX_DELAY);
                pbByteIndex = 0;

                // if message is not end properly
                if (protocolBuffor[pbMessageIndex][PROTOCOL_MESSAGE_SIZE - 1] != 0) {
                    xTimerStop(com.mReceiveMessageTimeoutTimer, portMAX_DELAY);
                    
                    Serial.print("BAD END OF MESSAGE ERROR! In receiveMessageTask() -> message should end with 0 (\\0 char), but got: ");
                    Serial.println(protocolBuffor[pbMessageIndex][PROTOCOL_MESSAGE_SIZE - 1]);

                    // TODO implement repeatMessage()
                    // repeatMessage();
                    resetProtocolBuffor();
                    xQueueReset(com.mReceiveByteQueue);
                } else {
                    // calculate checksum
                    uint16_t checksum = 0;
                    for (uint8_t i = 0; i < PROTOCOL_MESSAGE_SIZE; i++) {
                        checksum += protocolBuffor[pbMessageIndex][i];
                    }
                    
                    // if checksum is incorrect
                    if (checksum % 256 != 0) {
                        xTimerStop(com.mReceiveMessageTimeoutTimer, portMAX_DELAY);
                        Serial.println("BAD CHECKSUM ERROR! In receiveMessageTask() -> checksum incorrect");

                        // TODO implement repeatMessage()
                        // repeatMessage();
                        resetProtocolBuffor();
                        xQueueReset(com.mReceiveByteQueue);
                    }
                    // if MAC or IP is incorrect 
                    // TODO uncomment
                    // else if (!isProperMACAndIP(protocolBuffor[pbMessageIndex], protocolBuffor[pbMessageIndex][6])) {
                    else if (false) {
                        #ifdef CENTRAL_UNIT
                            Serial.println("CRITICAL ERROR! In receiveMessageTask() -> isProperMACAndIP() must never return false for CENTRAL_UNIT"); 
                        #else
                            xTimerStop(com.mReceiveMessageTimeoutTimer, portMAX_DELAY);
                            resetProtocolBuffor();
                        #endif
                    }
                    // if entire message is not ready (message quantity)
                    else if (protocolBuffor[pbMessageIndex][7] != 0) {
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
                            for (uint8_t i = 8; i < PROTOCOL_MESSAGE_SIZE - 2; i++) {
                                messageBuffor[messageIndex] = protocolBuffor[pbMessageIndex][i];
                                messageIndex++;
                                // protection against buffer overload (62 => 63 max buffer index -1 for \0)
                                if (messageIndex > 62) {
                                    // TODO add what to do with longer messages 
                                    break;
                                }
                            }

                            messagesQuantity = protocolBuffor[pbMessageIndex][7];
                            pbMessageIndex++;
                        } while(messagesQuantity != 0);

                        // TODO remove print ?
                        Serial.print("Received message: ");
                        uah::printArray(messageBuffor, MESSAGE_SIZE);

                        // TODO implement
                        // if it is "repeat" message
                        // if (isRepeatMessage(messageBuffor, messageIndex)) {
                        //     xSemaphoreTake(msLastMessageMutex, portMAX_DELAY);
                        //     xQueueSend(msSendMessagesQueue, msLastMessage, portMAX_DELAY);
                        //     xSemaphoreGive(msLastMessageMutex);
                        // } 
                        // if it is HC_12 command
                        #ifdef HC12_MODULE
                        //else
                         if (messageBuffor[0] == (uint8_t)'H' && messageBuffor[1] == (uint8_t)'C') {
                            com.mRfModule->setupHC12(messageBuffor);
                        }
                        #endif
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
            xTimerStart(com.mSuspendReceiveMessageTimer, portMAX_DELAY);
        }
    }
}

void Communication::createReceiveMessageTask() {
    if (mReceiveMessageTaskHandle == NULL) {
        xTaskCreate(
            receiveMessageTask,
            "Receive message",
            2048,
            NULL,
            LOW_TASK_PRIORITY,
            &mReceiveMessageTaskHandle
        );
    } else {
        Serial.println("TASK CREATION ERROR! In createReceiveMessageTask() -> Can't create receive message task, because task already exists");
    }
}
void Communication::deleteReceiveMessageTask() {
    if (mReceiveMessageTaskHandle != NULL) {
        vTaskDelete(mReceiveMessageTaskHandle);
        mReceiveMessageTaskHandle = NULL;
    }
}
// ================================================================

// ========================= Send Message =========================

void Communication::sendMessageTask(void *parameters) {
    auto &com = Communication::getInstance(Communication::mspDebugLED);

    // TODO change 2D protocolBuffor array for 1D array[PROTOCOL_MESSAGE_SIZE] and append ready protocol message to send queue in hc12 class
    // prepare message protocol buffor
    // [0-5{mac}, 6{ip}, 7{messagesQuantity}, 8-13{message}, 14{checksum}, 15{\0}]
    uint8_t protocolBuffor[PROTOCOL_MESSAGE_MAX_NUM][PROTOCOL_MESSAGE_SIZE];
    for (uint8_t i = 0; i < PROTOCOL_MESSAGE_MAX_NUM; i++){
        // TODO change to central unit MAC address
        // prepare MAC address
        for (uint8_t j = 0; j < 6; j++){
            protocolBuffor[i][j] = com.mMACAddress[j];
        }

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

        // prepare place for message quantity
        protocolBuffor[i][7] = 0;
        // prepare place for message
        for (uint8_t j = 8; j < PROTOCOL_MESSAGE_SIZE - 2; j++) {
            protocolBuffor[i][j] = BLANK_CHARACTER;
        }
        // prepare place checksum
        protocolBuffor[i][PROTOCOL_MESSAGE_SIZE - 2] = 0;
        // prepare '\0' (end of message)
        protocolBuffor[i][PROTOCOL_MESSAGE_SIZE - 1] = 0;
    }

    // prepare message to send buffor
    uint8_t messageBuffor[MESSAGE_SIZE];
    uah::prepareBuffor(messageBuffor, MESSAGE_SIZE);
    uint8_t messageIndex;
    int8_t messagesQuantity;

    // task loop
    for (;;) {
        messageIndex = 0;
        messagesQuantity = 0;

        // wait until the message appears in the queue and save message in local messageBuffor
        if (xQueueReceive(com.mSendMessagesQueue, &messageBuffor, pdMS_TO_TICKS(RECEIVE_BYTE_TIMEOUT)) == pdTRUE) {
            xTimerStop(com.mSuspendSendMessageTimer, portMAX_DELAY);

            // divide and add messages to protocolBuffor
            while(messageIndex < 64 && messageBuffor[messageIndex] != 0) {
                protocolBuffor[messagesQuantity][(messageIndex % 6) + 8] = messageBuffor[messageIndex];
                messageIndex++;
                if (messageIndex % 6 == 0) {
                    messagesQuantity++;
                }
            }
            if (messageIndex % 6 != 0){
                messagesQuantity++;
            }   

            uint8_t index = 0;
            while (messagesQuantity > 0) {
                messagesQuantity--;

                protocolBuffor[index][7] = messagesQuantity;

                protocolBuffor[index][PROTOCOL_MESSAGE_SIZE - 2] = 0;
                uint16_t checkSum = 0;
                for (uint8_t j = 0; j < PROTOCOL_MESSAGE_SIZE; j++) {
                    checkSum += (uint16_t)protocolBuffor[index][j];
                }
                checkSum = (256 - (checkSum % 256)) % 256;
                protocolBuffor[index][PROTOCOL_MESSAGE_SIZE - 2] = checkSum;

                index++;
            }

            // TODO remove print
            Serial.println("all sending...");

            // send message and clean buffor
            index = 0;
            do {
                // xTaskNotify(com.mCommunicationMainTaskHandle, sendingTaskWaitingNotif, eSetValueWithOverwrite);
                // xTimerStart(com.mReceiveByteTimeoutTimer, portMAX_DELAY);
                com.mRfModule->addMessageToTransmit(protocolBuffor[index]);
                // mspSerial->write(protocolBuffor[i], PROTOCOL_MESSAGE_SIZE);
                
                // TODO remove?
                // wait until hc12 module send confirmation
                // uint32_t hc12Respond;
                // if (xTaskNotifyWait(0, ULONG_MAX, &hc12Respond, pdMS_TO_TICKS(RECEIVE_BYTE_TIMEOUT * 2)) == pdTRUE) {
                //     xTimerStop(com.mReceiveByteTimeoutTimer, portMAX_DELAY);
                //     if (hc12Respond == 256){
                //         Serial.println("SENDING MESSAGE ERROR! In sendMessageTask() -> hc12 module is not responding.");
                //     } else if (hc12Respond != 255) {
                //         Serial.print("SENDING MESSAGE ERROR! In sendMessageTask() -> hc12 module did not confirm properly. Hc-12 module should send 255 signal but got: ");
                //         Serial.println(hc12Respond);
                //     }
                // } else {
                //     Serial.println("SENDING MESSAGE ERROR! In sendMessageTask() -> hc12 module is not responding.");
                // }

                messagesQuantity = protocolBuffor[index][7];
                for (uint8_t j = 8; j < PROTOCOL_MESSAGE_SIZE - 2; j++) {
                    protocolBuffor[index][j] = BLANK_CHARACTER;
                }
                protocolBuffor[index][PROTOCOL_MESSAGE_SIZE - 2] = 0;
                protocolBuffor[index][PROTOCOL_MESSAGE_SIZE - 1] = 0;
                
                index++;
            } while (messagesQuantity > 0);

            // TODO implement
            // messageBuffor is message to send, messageIndex is size of message (in loop is incremented 1 time too many, so now is size not index of array)
            // if (!isRepeatMessage(messageBuffor, messageIndex)) {
            //     setLastMessage(messageBuffor, messageIndex);
            // }

            xTimerStart(com.mSuspendSendMessageTimer, portMAX_DELAY);
        }
    }
}

void Communication::createSendMessageTask() {
    if (mSendMessageTaskHandle == NULL) {
        xTaskCreate(
            sendMessageTask,
            "Send Message",
            2048,
            NULL,
            MEDIUM_TASK_PRIORITY,
            &mSendMessageTaskHandle
        );
    } else {
        Serial.println("TASK CREATION ERROR! In createSendMessageTask() -> Can't create send message task, because task already exists");
    }
}

void Communication::deleteSendMessageTask() {
    if (mSendMessageTaskHandle != NULL) {
        vTaskDelete(mSendMessageTaskHandle);
        mSendMessageTaskHandle = NULL;
    }
}
// ================================================================

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
                uah::printArray(buffor, MESSAGE_SIZE);

                // check if is HC_12 command
                #ifdef HC12_MODULE
                    if (buffor[0] == 'A' && buffor[1] == 'T') {
                        buffor[0] = 'H';
                        buffor[1] = 'C';
                        com.mRfModule->setupHC12(buffor);
                    } else {
                        xQueueSend(com.mSendMessagesQueue, &buffor, portMAX_DELAY);
                    }
                #else
                    #error "not implemented"
                #endif

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
        xTaskNotify(com.mCommunicationMainTaskHandle, byteTimeoutNotif, eSetValueWithOverwrite);
    } else if (xTimer == com.mSuspendReceiveMessageTimer) {
        xTaskNotify(com.mCommunicationMainTaskHandle, suspendReceiveMessageTaskNotif, eSetValueWithOverwrite);
    } else if (xTimer == com.mSuspendSendMessageTimer) {
        xTaskNotify(com.mCommunicationMainTaskHandle, suspendSendMessageTaskNotif, eSetValueWithOverwrite);
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
    if (mSuspendReceiveMessageTimer == NULL) {
        mSuspendReceiveMessageTimer = xTimerCreate(
            "Suspend Receive Message",
            pdMS_TO_TICKS(SUSPEND_TASK_TIME_LONG),
            pdFALSE,
            NULL,
            communicationTimersCallbacks
        );
    }
    if (mSuspendSendMessageTimer == NULL) {
        mSuspendSendMessageTimer = xTimerCreate(
            "Suspend Send Message",
            pdMS_TO_TICKS(SUSPEND_TASK_TIME_LONG),
            pdFALSE,
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
    if (mSuspendReceiveMessageTimer != NULL) {
        xTimerDelete(mSuspendReceiveMessageTimer, portMAX_DELAY);
        mSuspendReceiveMessageTimer = NULL;
    }
    if (mSuspendSendMessageTimer != NULL) {
        xTimerDelete(mSuspendSendMessageTimer, portMAX_DELAY);
        mSuspendSendMessageTimer = NULL;
    }
}
// ================================================================