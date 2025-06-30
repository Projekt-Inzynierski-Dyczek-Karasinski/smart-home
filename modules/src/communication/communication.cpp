#include <HardwareSerial.h>
#include "communication/communication.h"
#include "smart_home_config.h"

#define MESSAGE_SIZE 64
#define PROTOCOL_MESSAGE_SIZE 16
#define BLANK_CHARACTER ' '
// TODO assign final value
#define SUSPEND_TASK_TIME 10000 // 10s
// TODO assign final value
#define RECEIVE_BYTE_TIMEOUT 100
// TODO assign final value
#define RECEIVE_MESSAGE_TIMEOUT 1000
// TODO assign final value
#define ADDRESSING_ABSOLUTE_TIMEOUT 60000 // 60s
// TODO assign final value
#define ADDRESSING_MESSAGE_TIMEOUT 1000 
// TODO assign final value
#define ADDRESSING_MAX_ATTEMPTS 5

uint8_t Communication::msMACAddress[6];
HardwareSerial* Communication::mspSerial = nullptr;
DebugLED* Communication::mspDebugLED = nullptr;

uint8_t Communication::msLastMessage[MESSAGE_SIZE];
SemaphoreHandle_t Communication::msLastMessageMutex = NULL;

TaskHandle_t Communication::msCommunicationMainTaskHandle = NULL;

TaskHandle_t Communication::msReceiveMessageTaskHandle = NULL;
// TODO remove "sendCustomMessage" methods
TaskHandle_t Communication::msSendCustomMessageTaskHandle = NULL;
TaskHandle_t Communication::msSendMessageTaskHandle = NULL;
TaskHandle_t Communication::msAddressingTaskHandle = NULL;
TaskHandle_t Communication::msSetupHC12TaskHandle = NULL;

QueueHandle_t Communication::msReceiveMessageQueue = NULL;
QueueHandle_t Communication::msReceiveByteQueue = NULL;
QueueHandle_t Communication::msSendMessagesQueue = NULL;

TimerHandle_t Communication::msReceiveMessageTimeoutTimer = NULL;
TimerHandle_t Communication::msReceiveByteTimeoutTimer = NULL;
TimerHandle_t Communication::msSuspendReceiveMessageTimer = NULL;
TimerHandle_t Communication::msSuspendSendMessageTimer = NULL;

TimerHandle_t Communication::msAddressingTimeoutTimer = NULL;

#ifdef CENTRAL_UNIT
    Communication::Routing Communication::msRoutingTable[255];
#else
    static uint8_t msCentralUnitMACAddress[6];
    static uint8_t msIPAddress = 0;
#endif

Communication::Communication(DebugLED *debugLED) {
    // Get MAC address
    #ifdef ESP32_BOARD
        esp_read_mac(msMACAddress, ESP_MAC_WIFI_STA);
    #else
        // TODO add function to get MAC address on different boards
        #error "MAC address not implemented!"
    #endif 

    #ifdef CENTRAL_UNIT
        for (uint8_t i = 0; i < 255; i++) {
            msRoutingTable[i].IPAddress = 0;
        }
    #endif

    msLastMessageMutex = xSemaphoreCreateMutex();

    xSemaphoreTake(msLastMessageMutex, portMAX_DELAY);
    for (uint8_t i = 0; i < MESSAGE_SIZE; i++) {
        msLastMessage[i] = 0;
    }
    xSemaphoreGive(msLastMessageMutex);

    mspDebugLED = debugLED;

    pinMode(SET_PIN, OUTPUT);
    digitalWrite(SET_PIN, HIGH);
    
    mspSerial = new HardwareSerial(HARDWARE_SERIAL_UART_NR);
    mspSerial->begin((unsigned long)BAUD_RATE, SERIAL_8N1, RX_PIN, TX_PIN);

    createCommunicationQueues();
    createCommunicationTimers();

    // TODO remove "sendCustomMessage" methods
    createSendCustomMessageTask();
    createSendMessageTask();
    vTaskSuspend(msSendMessageTaskHandle);
    createReceiveMessageTask();
    vTaskSuspend(msReceiveMessageTaskHandle);

    createCommunicationMainTask();

    
    Serial.println("Communication initialized");
    // TODO remove
    // vTaskDelay(pdMS_TO_TICKS(1000));
    // xTaskNotify(msCommunicationMainTaskHandle, createSetupHC12TaskNotif, eSetValueWithOverwrite);
}

Communication::~Communication() {
    deleteCommunicationMainTask();
    // TODO remove "sendCustomMessage" methods
    deleteSendCustomMessageTask();
    deleteSendMessageTask();

    deleteReceiveMessageTask();
    deleteAddressingTask();


    deleteCommunicationQueues();

    deleteCommunicationTimers();
    deleteAddresingTimer();

    delete mspSerial;
    digitalWrite(SET_PIN, LOW);
}

void Communication::startAddresingAlgorithm() {
    // TODO remove print
    Serial.println("startAddresingAlgorithm");
    xTaskNotify(msCommunicationMainTaskHandle, createAddressingTaskNotif, eSetValueWithOverwrite);
}

// ====================== Communication Main ======================

void Communication::communicationMainTask() {
    uint32_t status = defaultStatusNotif;

    bool isSendingTaskWaiting = false;
    bool isSetupHC12Working = false;
    uint8_t hc12Input;
    
    for (;;) {
        // change status
        xTaskNotifyWait(0, ULONG_MAX, &status, 0);

        switch (status) {
            case defaultStatusNotif:
                if (mspSerial->available()) {
                    hc12Input = mspSerial->read();
                    if (isSetupHC12Working) {
                        Serial.println((char)hc12Input);
                    } else {
                        if (isSendingTaskWaiting) {
                            xTaskNotify(msSendMessageTaskHandle, (uint32_t)hc12Input, eSetValueWithOverwrite);
                            isSendingTaskWaiting = false;
                            // TODO is this essential?
                            vTaskPrioritySet(msCommunicationMainTaskHandle, BACKGROUND_TASK_PRIORITY);
                            // TODO remove print
                            // Serial.println("vTaskPrioritySet(msCommunicationMainTaskHandle, BACKGROUND_TASK_PRIORITY);");
                        } else {
                            xQueueSend(msReceiveByteQueue, &hc12Input, portMAX_DELAY);                       
                        }
                    }    
                }
                if (!isSetupHC12Working) {
                    if (eTaskGetState(msReceiveMessageTaskHandle) == eSuspended && uxQueueMessagesWaiting(msReceiveByteQueue) != 0) {
                        // TODO remove print
                        Serial.println("vTaskResume(msReceiveMessageTaskHandle);");
                        vTaskResume(msReceiveMessageTaskHandle);
                    }
                    if (eTaskGetState(msSendMessageTaskHandle) == eSuspended && uxQueueMessagesWaiting(msSendMessagesQueue) != 0) {
                        // TODO remove print
                        Serial.println("vTaskResume(msSendMessageTaskHandle);");
                        vTaskResume(msSendMessageTaskHandle);
                    }
                }    

                // delay for watchdog 
                vTaskDelay(pdMS_TO_TICKS(1));
                break;

            case sendingTaskWaitingNotif:
                // TODO is this essential?
                vTaskPrioritySet(msCommunicationMainTaskHandle, HIGH_TASK_PRIORITY);
                // TODO remove print
                // Serial.println("vTaskPrioritySet(msCommunicationMainTaskHandle, HIGH_TASK_PRIORITY);");
                isSendingTaskWaiting = true;
                break;
                
            case byteTimeoutNotif:
                if (isSendingTaskWaiting) {
                    // max value that hc12 can send is 255, so notifying Send Message Task with 256 value mean timeout
                    xTaskNotify(msSendMessageTaskHandle, (uint32_t)256, eSetValueWithOverwrite);
                    isSendingTaskWaiting = false;
                    // TODO is this essential?
                    vTaskPrioritySet(msCommunicationMainTaskHandle, BACKGROUND_TASK_PRIORITY);
                    // TODO remove print
                    // Serial.println("vTaskPrioritySet(msCommunicationMainTaskHandle, BACKGROUND_TASK_PRIORITY);");
                } else {
                    xTaskNotify(msReceiveMessageTaskHandle, byteTimeoutNotif, eSetValueWithoutOverwrite);
                }
                break;

            case messageTimeoutNotif:
                xTaskNotify(msReceiveMessageTaskHandle, messageTimeoutNotif, eSetValueWithoutOverwrite);
                break;

            case readRawMessageNotif:
                xTaskNotify(msReceiveMessageTaskHandle, readRawMessageNotif, eSetValueWithoutOverwrite);
                break;

            case suspendReceiveMessageTaskNotif:
                // TODO remove print
                Serial.println("vTaskSuspend(msReceiveMessageTaskHandle)");
                vTaskSuspend(msReceiveMessageTaskHandle);
                break;
            
            case suspendSendMessageTaskNotif:
                // TODO remove print
                Serial.println("vTaskSuspend(msSendMessageTaskHandle);");
                resetLastMessage();
                vTaskSuspend(msSendMessageTaskHandle);
                break;

            case createAddressingTaskNotif:
                createAddressingTask();
                break;

            case deleteAddressingTaskNotif:
                deleteAddressingTask();
                break;

            case deleteAddressingTaskWithAbortNotif:
                abortAddressing();
                break;

            case createSetupHC12TaskNotif:
                // TODO remove print
                Serial.println("createSetupHC12Task();");

                isSetupHC12Working = true;
                xTimerStop(msSuspendSendMessageTimer, portMAX_DELAY);
                vTaskSuspend(msSendMessageTaskHandle);
                createSetupHC12Task();
                break;

            case deleteSetupHC12TaskNotif:
                // TODO remove print
                Serial.println("deleteSetupHC12Task();");
                deleteSetupHC12Task();
                isSetupHC12Working = false;
                break;
            
            default:
                Serial.print("STATUS ERROR! In communicationMainTask() -> got unknow status. Received Status: ");
                Serial.println(status);
                break;
        }

        // reset notifications status 
        status = defaultStatusNotif;
    }
    
    // for(;;) {
    //     // change status
    //     xTaskNotifyWait(0, ULONG_MAX, &status, 0);

    //     switch (status) {
    //         case mCommunicationMainNotifications::defaultStatus:
    //             if (mspSerial->available()) {
    //                 // Serial.print("defaultStatus: ");
    //                 // Serial.println(isSendingTaskWaiting);

    //                 if (isSendingTaskWaiting) {
    //                     xTimerStop(msReceiveByteTimeoutTimer, portMAX_DELAY);
    //                     hc12Input = mspSerial->read();
    //                     xTaskNotify(msSendMessageTaskHandle, (uint32_t)hc12Input, eSetValueWithOverwrite);
    //                     isSendingTaskWaiting = false;
    //                 } else {
    //                     hc12Input = mspSerial->read();
    //                     xQueueSend(msReceiveByteQueue, &hc12Input, portMAX_DELAY);
    //                     xTimerStart(msReceiveByteTimeoutTimer, portMAX_DELAY);
    //                 }
    //             }
    //             break;

    //         case mCommunicationMainNotifications::byteTimeout:
    //             if (isSendingTaskWaiting) {
    //                 // max value that hc12 can send is 255, so notifying Send Message Task with 256 value mean timeout
    //                 xTaskNotify(msSendMessageTaskHandle, 256, eSetValueWithOverwrite);
    //                 isSendingTaskWaiting = false;
    //                 status = mCommunicationMainNotifications::defaultStatus;
    //             } else {
    //                 xTaskNotify(msReceiveMessageTaskHandle, mCommunicationMainNotifications::byteTimeout, eSetValueWithOverwrite);
    //                 // Sending queue to "awake" task
    //                 xQueueSend(msReceiveByteQueue, &hc12Input, portMAX_DELAY);
    //                 status = mCommunicationMainNotifications::defaultStatus;
    //             }
    //             break;

    //         case mCommunicationMainNotifications::sendingTaskWaiting: 
    //             if (!isSendingTaskWaiting) {
    //                 isSendingTaskWaiting = true;
    //                 xTimerStart(msReceiveByteTimeoutTimer, portMAX_DELAY);
    //                 status = mCommunicationMainNotifications::defaultStatus;
    //             }
    //             break;

    //         default:
    //             Serial.print("STATUS ERROR! In communicationMainTask() -> got unknow status. Received Status: ");
    //             Serial.println(status);
    //             isSendingTaskWaiting = false;
    //             status = mCommunicationMainNotifications::defaultStatus;
    //     }

    //     // delay for watchdog
    //     vTaskDelay(pdMS_TO_TICKS(1));
    // }
}
void Communication::createCommunicationMainTaskHandle(void *parameters) {
    Communication* instance = static_cast<Communication*>(parameters);
    instance->communicationMainTask();
}
void Communication::createCommunicationMainTask() {
    if (msCommunicationMainTaskHandle == NULL) {
        xTaskCreate(
            createCommunicationMainTaskHandle,
            "Communication Main",
            2048,
            NULL,
            BACKGROUND_TASK_PRIORITY,
            &msCommunicationMainTaskHandle
        );
    } else {
        Serial.println("TASK CREATION ERROR! In createCommunicationMainTask() -> Can't create Communication Main task, because task already exists");
    }
}
void Communication::deleteCommunicationMainTask() {
    if (msCommunicationMainTaskHandle != NULL) {
        vTaskDelete(msCommunicationMainTaskHandle);
        msCommunicationMainTaskHandle = NULL;
    }
}
// ================================================================

// ============================ Queues ============================

void Communication::createCommunicationQueues() {
    if (msReceiveMessageQueue == NULL) {
        msReceiveMessageQueue = xQueueCreate(10, sizeof(uint8_t[MESSAGE_SIZE]));
    }
    if (msReceiveByteQueue == NULL) {
        // TODO assign propper length of queue 
        msReceiveByteQueue = xQueueCreate(128, sizeof(uint8_t));
    }
    if (msSendMessagesQueue == NULL) {
        msSendMessagesQueue = xQueueCreate(10, sizeof(uint8_t[MESSAGE_SIZE]));
    }
}
void Communication::deleteCommunicationQueues() {
    if (msReceiveMessageQueue != NULL) {
        vQueueDelete(msReceiveMessageQueue);
        msReceiveMessageQueue = NULL;
    }
    if (msReceiveByteQueue != NULL) {
        vQueueDelete(msReceiveByteQueue);
        msReceiveByteQueue = NULL;
    }
    if (msSendMessagesQueue != NULL) {
        vQueueDelete(msSendMessagesQueue);
        msSendMessagesQueue = NULL;
    }
}

// ================================================================

// ======================= Receive Message ========================

void Communication::receiveMessageTask() {
    // timeout status/cause
    uint32_t timeoutStatus = defaultStatusNotif;
    // if true receiveMessageTask() will put in xQueueSend() protocolBuffor[0] insted messageBuffor
    bool isRawMessage = false;

    // prepare message protocol buffor
    // [0-5{mac}, 6{ip}, 7{messagesQuantity}, 8-13{message}, 14{checksum}, 15{\0}]
    uint8_t protocolBuffor[11][PROTOCOL_MESSAGE_SIZE];

    // protocolBufforMessageIndex
    uint8_t pbMessageIndex = 0;

    // protocolBufforByteIndex
    uint8_t pbByteIndex = 0;

    // lambda function for clearing buffor
    auto resetProtocolBuffor = [&]() {
        for (uint8_t i = 0; i < 11; i++){
            for (uint8_t j = 0; j < PROTOCOL_MESSAGE_SIZE; j++) {
                protocolBuffor[i][j] = 0;
            }
        }
        pbByteIndex = 0;
        pbMessageIndex = 0;
    };
    resetProtocolBuffor();

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
                    xTimerStop(msReceiveMessageTimeoutTimer, portMAX_DELAY);
                    // TODO remove print
                    Serial.println("Byte timeout");
                    resetProtocolBuffor();
                    xQueueReset(msReceiveByteQueue);
                } else if (timeoutStatus == messageTimeoutNotif) {
                    xTimerStop(msReceiveByteTimeoutTimer, portMAX_DELAY);
                    // TODO remove print
                    Serial.println("Message timeout");
                    resetProtocolBuffor();
                    xQueueReset(msReceiveByteQueue);
                } else {
                    Serial.print("STATUS ERROR! In receiveMessageTask() -> got unknow status. Received Status: ");
                    Serial.println(timeoutStatus);
                }
            }
            timeoutStatus = defaultStatusNotif;
        } 
        // decoding message
        if (xQueueReceive(msReceiveByteQueue, &queueBuffor, pdMS_TO_TICKS(RECEIVE_BYTE_TIMEOUT)) == pdTRUE) {
            xTimerStop(msSuspendReceiveMessageTimer, portMAX_DELAY);
            protocolBuffor[pbMessageIndex][pbByteIndex] = queueBuffor;
            // if new message start message timeout timer
            if (pbByteIndex == 0 && pbMessageIndex == 0){
                xTimerStart(msReceiveMessageTimeoutTimer, portMAX_DELAY);
            }
            // if protocol message is not complete
            if (pbByteIndex != PROTOCOL_MESSAGE_SIZE - 1) {
                pbByteIndex++;
                xTimerStart(msReceiveByteTimeoutTimer, portMAX_DELAY);
            } else {
                xTimerStop(msReceiveByteTimeoutTimer, portMAX_DELAY);
                pbByteIndex = 0;

                // if message is not end properly
                if (protocolBuffor[pbMessageIndex][PROTOCOL_MESSAGE_SIZE - 1] != 0) {
                    xTimerStop(msReceiveMessageTimeoutTimer, portMAX_DELAY);
                    
                    Serial.print("BAD END OF MESSAGE ERROR! In receiveMessageTask() -> message should end with 0 (\\0 char), but got: ");
                    Serial.println(protocolBuffor[pbMessageIndex][PROTOCOL_MESSAGE_SIZE - 1]);

                    repeatMessage();
                    resetProtocolBuffor();
                    xQueueReset(msReceiveByteQueue);
                }
                else {
                    // calculate checksum
                    uint16_t checksum = 0;
                    for (uint8_t i = 0; i < PROTOCOL_MESSAGE_SIZE; i++) {
                        checksum += protocolBuffor[pbMessageIndex][i];
                    }
                    
                    // if checksum is incorrect
                    if (checksum % 256 != 0) {
                        xTimerStop(msReceiveMessageTimeoutTimer, portMAX_DELAY);
                        Serial.println("BAD CHECKSUM ERROR! In receiveMessageTask() -> checksum incorrect");

                        repeatMessage();
                        resetProtocolBuffor();
                        xQueueReset(msReceiveByteQueue);
                    }
                    // if MAC or IP is incorrect 
                    // TODO add check for MAC and IP addresses
                    else if (false) {
                        // TODO add what should happen if MAC or IP is incorrect 
                    }
                    // if entire message is not ready (message quantity)
                    else if (protocolBuffor[pbMessageIndex][7] != 0) {
                        pbMessageIndex++;
                    }
                    // entire message is ready
                    else {
                        xTimerStop(msReceiveMessageTimeoutTimer, portMAX_DELAY);

                        // prepare received message buffor
                        uint8_t messageBuffor[MESSAGE_SIZE];
                        for (uint8_t i = 0; i < MESSAGE_SIZE; i++){
                            messageBuffor[i] = 0;
                        }
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

                        Serial.print("Received message: ");
                        Serial.println((char*)messageBuffor);

                        // if it is "repeat" message
                        if (isRepeatMessage(messageBuffor, messageIndex)) {
                            xSemaphoreTake(msLastMessageMutex, portMAX_DELAY);
                            xQueueSend(msSendMessagesQueue, msLastMessage, portMAX_DELAY);
                            xSemaphoreGive(msLastMessageMutex);
                        }
                        // if Addressing Task is working
                        else if (msAddressingTaskHandle != NULL) {
                            if (isRawMessage) {
                                xQueueSend(msReceiveMessageQueue, protocolBuffor[0], portMAX_DELAY);
                                isRawMessage = false;
                            } else {
                                xQueueSend(msReceiveMessageQueue, messageBuffor, portMAX_DELAY);
                            }
                        }
                        // TODO add message to queue
                        // else

                        // clean up
                        resetProtocolBuffor();
                    }
                }
            }
            xTimerStart(msSuspendReceiveMessageTimer, portMAX_DELAY);
        }
    }
}
void Communication::createReceiveMessageTaskHandle(void *parameters) {
    Communication* instance = static_cast<Communication*>(parameters);
    instance->receiveMessageTask();
}
void Communication::createReceiveMessageTask() {
    createCommunicationTimers();

    if (msReceiveMessageTaskHandle == NULL) {
        xTaskCreate(
            createReceiveMessageTaskHandle,
            "Receive message",
            2048,
            NULL,
            LOW_TASK_PRIORITY,
            &msReceiveMessageTaskHandle
        );
    } else {
        Serial.println("TASK CREATION ERROR! In createReceiveMessageTask() -> Can't create receive message task, because task already exists");
    }
}
void Communication::deleteReceiveMessageTask() {
    if (msReceiveMessageTaskHandle != NULL) {
        vTaskDelete(msReceiveMessageTaskHandle);
        msReceiveMessageTaskHandle = NULL;
    }

    deleteCommunicationTimers();
}
// ================================================================

// TODO remove "sendCustomMessage" methods

// ===================== Send Custom Message ======================

void Communication::sendCustomMessageTask() {
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
                uint8_t i = 0;
                Serial.print("Message Ready: ");
                while (buffor[i] != 0) {
                    Serial.print((char)buffor[i]);
                    i++;
                }
                Serial.println();

                // add message to SendMessagesQueue
                xQueueSend(msSendMessagesQueue, &buffor, portMAX_DELAY);

                // reset buffor
                for (uint8_t i = 0; i < index; i++){
                    buffor[i] = 0;
                }
                index = 0;
            }
        }
        // delay for watchdog
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
void Communication::createSendCustomMessageTaskHandle(void *parameters) {
    Communication* instance = static_cast<Communication*>(parameters);
    instance->sendCustomMessageTask();
}
void Communication::createSendCustomMessageTask() {
    if (msSendCustomMessageTaskHandle == NULL) {
        xTaskCreate(
            createSendCustomMessageTaskHandle,
            "Send custom message",
            2048,
            NULL,
            BACKGROUND_TASK_PRIORITY,
            &msSendCustomMessageTaskHandle
        );
    } else {
        Serial.println("TASK CREATION ERROR! In createSendCustomMessageTask() -> Can't create send custom message task, because task already exists");
    }
}
void Communication::deleteSendCustomMessageTask() {
    if (msSendCustomMessageTaskHandle != NULL) {
        vTaskDelete(msSendCustomMessageTaskHandle);
        msSendCustomMessageTaskHandle = NULL;
    }
}
// ================================================================

// ========================= Send Message =========================

void Communication::sendMessageTask() {
    // prepare message protocol buffor
    // [0-5{mac}, 6{ip}, 7{messagesQuantity}, 8-13{message}, 14{checksum}, 15{\0}]
    uint8_t protocolBuffor[11][PROTOCOL_MESSAGE_SIZE];
    for (uint8_t i = 0; i < 11; i++){
        // TODO change to central unit MAC address
        // prepare MAC address
        for (uint8_t j = 0; j < 6; j++){
            protocolBuffor[i][j] = msMACAddress[j];
        }
        // TODO change to IP address
        // prepare IP address
        protocolBuffor[i][6] = 255;
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
    for (uint8_t i = 0; i < MESSAGE_SIZE; i++){
        messageBuffor[i] = 0;
    }
    uint8_t messageIndex;
    int8_t messagesQuantity;

    // task loop
    for (;;) {
        messageIndex = 0;
        messagesQuantity = 0;

        // wait until the message appears in the queue and save message in local messageBuffor
        if (xQueueReceive(msSendMessagesQueue, &messageBuffor, pdMS_TO_TICKS(RECEIVE_BYTE_TIMEOUT)) == pdTRUE) {
            xTimerStop(msSuspendSendMessageTimer, portMAX_DELAY);

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

            uint8_t i = 0;
            while (messagesQuantity > 0) {
                messagesQuantity--;

                protocolBuffor[i][7] = messagesQuantity;

                protocolBuffor[i][PROTOCOL_MESSAGE_SIZE - 2] = 0;
                uint16_t checkSum = 0;
                for (uint8_t j = 0; j < PROTOCOL_MESSAGE_SIZE; j++) {
                    checkSum += (uint16_t)protocolBuffor[i][j];
                }
                checkSum = (256 - (checkSum % 256)) % 256;
                protocolBuffor[i][PROTOCOL_MESSAGE_SIZE - 2] = checkSum;

                i++;
            }

            // TODO remove print
            Serial.println("all sending...");

            // send message and clean buffor
            i = 0;
            do {
                xTaskNotify(msCommunicationMainTaskHandle, sendingTaskWaitingNotif, eSetValueWithOverwrite);
                xTimerStart(msReceiveByteTimeoutTimer, portMAX_DELAY);
                mspSerial->write(protocolBuffor[i], PROTOCOL_MESSAGE_SIZE);
                
                // wait until hc12 module send confirmation
                uint32_t hc12Respond;
                if (xTaskNotifyWait(0, ULONG_MAX, &hc12Respond, pdMS_TO_TICKS(RECEIVE_BYTE_TIMEOUT * 2)) == pdTRUE) {
                
                    xTimerStop(msReceiveByteTimeoutTimer, portMAX_DELAY);
                    if (hc12Respond == 256){
                        Serial.println("SENDING MESSAGE ERROR! In sendMessageTask() -> hc12 module is not responding.");
                    } else if (hc12Respond != 255) {
                        Serial.print("SENDING MESSAGE ERROR! In sendMessageTask() -> hc12 module did not confirm properly. Hc-12 module should send 255 signal but got: ");
                        Serial.println(hc12Respond);
                    }
                } else {
                    Serial.println("SENDING MESSAGE ERROR! In sendMessageTask() -> hc12 module is not responding.");
                }
                // TODO remove 
                // Serial.print("Message ");
                // Serial.print(i);
                // Serial.print(": ");
                // for (uint8_t j = 0; j < 16; j++){
                //     Serial.print((int)protocolBuffor[i][j]);
                //     Serial.print(" ");
                // }
                // Serial.println();

                messagesQuantity = protocolBuffor[i][7];
                for (uint8_t j = 8; j < PROTOCOL_MESSAGE_SIZE - 2; j++) {
                    protocolBuffor[i][j] = BLANK_CHARACTER;
                }
                protocolBuffor[i][PROTOCOL_MESSAGE_SIZE - 2] = 0;
                protocolBuffor[i][PROTOCOL_MESSAGE_SIZE - 1] = 0;
                
                i++;
                // this delay is required for HC12 send/receive properly message
                // TODO increase delay?
                vTaskDelay(pdMS_TO_TICKS(25));
            } while (messagesQuantity > 0);

            bool isReapeatMessage = true;
            if (messageIndex != 6) {
                isReapeatMessage = false;
            } else {
                uint8_t repeat[] = {'r', 'e', 'p', 'e', 'a', 't'}; 
                for (uint8_t j = 0; j < 6; j++){
                    if (messageBuffor[j] != repeat[j]) {
                        isReapeatMessage = false;
                        break;
                    }
                }
            }

            // messageBuffor is message to send, messageIndex is size of message (in loop is incremented 1 time too many, so now is size not index of array)
            if (!isRepeatMessage(messageBuffor, messageIndex)) {
                setLastMessage(messageBuffor, messageIndex);
            }

            xTimerStart(msSuspendSendMessageTimer, portMAX_DELAY);
        }
    }
}
void Communication::createSendMessageTaskHandle(void *parameters) {
    Communication* instance = static_cast<Communication*>(parameters);
    instance->sendMessageTask();
}
void Communication::createSendMessageTask() {
    if (msSendMessageTaskHandle == NULL) {
        xTaskCreate(
            createSendMessageTaskHandle,
            "Send Message",
            2048,
            NULL,
            HIGH_TASK_PRIORITY,
            &msSendMessageTaskHandle
        );
    } else {
        Serial.println("TASK CREATION ERROR! In createSendMessageTask() -> Can't create send message task, because task already exists");
    }
}
void Communication::deleteSendMessageTask() {
    if (msSendMessageTaskHandle != NULL) {
        vTaskDelete(msSendMessageTaskHandle);
        msSendMessageTaskHandle = NULL;
    }
}
// ================================================================

// ========================== Addressing ==========================

void Communication::abortAddressing() {
    uint8_t sendBuffor[MESSAGE_SIZE];
    sendBuffor[0] = (uint8_t)'a';
    sendBuffor[1] = (uint8_t)'b';
    sendBuffor[2] = (uint8_t)'o';
    sendBuffor[3] = (uint8_t)'r';
    sendBuffor[4] = (uint8_t)'t';
    for (uint8_t i = 5; i < MESSAGE_SIZE; i++){
        sendBuffor[i] = 0;
    }

    // TODO remove print
    Serial.println("abortAddressing()");
    for (uint8_t i = 0; i < 3; i++) {
        xQueueSend(msSendMessagesQueue, &sendBuffor, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    xTaskNotify(msCommunicationMainTaskHandle, deleteAddressingTaskNotif, eSetValueWithOverwrite);
}

void Communication::addressingTask() {
    xTimerStart(msAddressingTimeoutTimer, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(6000));
    xTaskNotify(msCommunicationMainTaskHandle, deleteAddressingTaskNotif, eSetValueWithOverwrite);
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// #error "implement void Communication::addressingTask() "
// #ifdef CENTRAL_UNIT
// void Communication::addressingTask() {
//     enum addresingStageEnum : uint8_t {
//         newConnection = 0,
//         newRFChannel = 1,
//         changeRFChannel = 2,
//         summation = 3,
//         oldConfiguration = 4
//     };
//     uint8_t addresingStage = addresingStageEnum::newConnection;
//     // 0 new connection - request for new connection
//     // 1 ping - ping
//     // 2 reply ping - replay to ping
//     // 3 abort - abort new connection
//     // 4 summary - after this message central unit will send all data about connection for verification 
//     // 5 ok summary - confirmation for data sent in summary
//     // 6 wrong summary - rejection for data sent in summary
//     // 7 ip [number] - reply for "new connection" with new ip for module. WARNING! [number] have to be set separately!!! 
//     // n yes rf channels, yes transition (without) permission - can change rf channels,   want to       transmit without permission
//     // n yes rf channels, no  transition (without) permission - can change rf channels,   don't want to transmit without permission
//     // n no  rf channels, yes transition (without) permission - can't change rf channels, want to       transmit without permission
//     // n no  rf channels, no  transition (without) permission - can't change rf channels, don't want to transmit without permission
//     const uint8_t MESSAGES[][6] = {
//         {'n', 'e', 'w', 'c', 'o', 'n'}, 
//         {'p', 'i', 'n', 'g', '\0', '\0'}, 
//         {'r', 'e', 'p', 'i', 'n', 'g'}, 
//         {'a', 'b', 'o', 'r', 't', '\0'},
//         {'s', 'u', 'm', 'm', 'a', 'r'},
//         {'o', 'k', 's', 'u', 'm', 'm'},
//         {'w', 'r', 's', 'u', 'm', 'm'},
//         {'i', 'p', '\0', '\0', '\0', '\0'},
//         {'y', 'r', 'c', 'y', 't', 'p'}, 
//         {'y', 'r', 'c', 'n', 't', 'p'}, 
//         {'n', 'r', 'c', 'y', 't', 'p'}, 
//         {'n', 'r', 'c', 'n', 't', 'p'}, 
//     };
//     uint8_t attemptCounter = 0;

//     // prepare receive buffor
//     uint8_t receiveBuffor[MESSAGE_SIZE];
//     for (uint8_t i = 0; i < MESSAGE_SIZE; i++){
//         receiveBuffor[i] = 0;
//     }

//     // prepare send buffor
//     uint8_t sendBuffor[MESSAGE_SIZE];
//     for (uint8_t i = 0; i < MESSAGE_SIZE; i++){
//         sendBuffor[i] = 0;
//     }
//     // // lambda function for setting message in sendBuffor
//     // auto setSendBuffor = [&](uint8_t* message) {
//     //     for (uint8_t i = 0; i < 6; i++){
//     //         sendBuffor[i] = message[i];
//     //     }
//     // };
    
//     xTimerStart(msAddressingTimeoutTimer, portMAX_DELAY);
//     for(;;) {
//         if (attemptCounter > ADDRESSING_MAX_ATTEMPTS) {
//             Serial.println("CONNECTION ERROR! In addressingTask() -> Exceeded max number of connection attempts");
//             // TODO add CONNECTION ERROR DebugLED blink
//             for (uint8_t i = 0; i < 6; i++){
//                 sendBuffor[i] = MESSAGES[3][i];
//             }
//             for (uint8_t i = 0; i < 3; i++) {
//                 xQueueSend(msSendMessagesQueue, &sendBuffor, portMAX_DELAY);
//                 vTaskDelay(pdMS_TO_TICKS(300));
//             }
//             deleteAddressingTask();
//         } else {
//             attemptCounter++;

//             switch (addresingStage) {
//                 case addresingStageEnum::newConnection:
//                     for (uint8_t i = 0; i < 6; i++){
//                         sendBuffor[i] = MESSAGES[0][i];
//                     }
//                     xTaskNotify(msReceiveMessageTaskHandle, mCommunicationMainNotifications::readRawMessage, eSetValueWithOverwrite);
//                     if (xQueueReceive(msReceiveMessageQueue, &receiveBuffor, pdMS_TO_TICKS(ADDRESSING_ABSOLUTE_TIMEOUT)) == pdTRUE) {
//                         Serial.println("addresingStageEnum::newConnection");
//                         Serial.print("last rec message: ");
//                         for (uint8_t i = 0; i < 16; i++) {
//                             Serial.print(receiveBuffor[i]);
//                             Serial.print(' ');
//                         }
//                         Serial.println();
//                         for (uint8_t i = 0; i < 6; i++){
//                             sendBuffor[i] = MESSAGES[7][i];
//                         }
//                         sendBuffor[2] = 'T';
//                         vTaskDelay(pdMS_TO_TICKS(25));
//                         xQueueSend(msSendMessagesQueue, &sendBuffor, portMAX_DELAY);

//                         // #ifdef RF_CHANNELS
//                         //     addresingStage = addresingStageEnum::newRFChannel;
//                         // #else
//                         //     addresingStage = addresingStageEnum::summation;
//                         // #endif
//                         attemptCounter = 0;
//                     }
//                     break;
                
//                 case addresingStageEnum::newRFChannel:
//                     vTaskDelay(pdMS_TO_TICKS(5000));
//                     // Serial.println("addresingStageEnum::newRFChannel");
//                     // Serial.print("last rec message: ");
//                     // for (uint8_t i = 0; i < 16; i++) {
//                     //     if (receiveBuffor[i] == '\0') {
//                     //         break;
//                     //     }
//                     //     Serial.print(receiveBuffor[i]);
//                     //     Serial.print(' ');
//                     // }
//                     // Serial.println();
//                     attemptCounter = 255;
//                     break;
                
//                 default:
//                     Serial.print("ADDRESING STAGE ERROR! In addressingTask() -> unknow addresingStage. AddresingStage: ");
//                     Serial.println(addresingStage);
//                     break;
//             }
//         }
//     }
// }
// #else
// void Communication::addressingTask() {
//     enum addresingStageEnum : uint8_t {
//         newConnection = 0,
//         newRFChannel = 1,
//         changeRFChannel = 2,
//         summation = 3,
//         oldConfiguration = 4
//     };
//     uint8_t addresingStage = addresingStageEnum::newConnection;
//     // 0 new connection - request for new connection
//     // 1 ping - ping
//     // 2 reply ping - replay to ping
//     // 3 abort - abort new connection
//     // 4 summary - after this message central unit will send all data about connection for verification 
//     // 5 ok summary - confirmation for data sent in summary
//     // 6 wrong summary - rejection for data sent in summary
//     // n yes rf channels, yes transition (without) permission - can change rf channels,   want to       transmit without permission
//     // n yes rf channels, no  transition (without) permission - can change rf channels,   don't want to transmit without permission
//     // n no  rf channels, yes transition (without) permission - can't change rf channels, want to       transmit without permission
//     // n no  rf channels, no  transition (without) permission - can't change rf channels, don't want to transmit without permission
//     const uint8_t MESSAGES[][6] = {
//         {'n', 'e', 'w', 'c', 'o', 'n'}, 
//         {'p', 'i', 'n', 'g', '\0', '\0'}, 
//         {'r', 'e', 'p', 'i', 'n', 'g'}, 
//         {'a', 'b', 'o', 'r', 't', '\0'},
//         {'s', 'u', 'm', 'm', 'a', 'r'},
//         {'o', 'k', 's', 'u', 'm', 'm'},
//         {'w', 'r', 's', 'u', 'm', 'm'},
//         {'y', 'r', 'c', 'y', 't', 'p'}, 
//         {'y', 'r', 'c', 'n', 't', 'p'}, 
//         {'n', 'r', 'c', 'y', 't', 'p'}, 
//         {'n', 'r', 'c', 'n', 't', 'p'}, 
//     };
//     uint8_t attemptCounter = 0;

//     // prepare receive buffor
//     uint8_t receiveBuffor[MESSAGE_SIZE];
//     for (uint8_t i = 0; i < MESSAGE_SIZE; i++){
//         receiveBuffor[i] = 0;
//     }

//     // prepare send buffor
//     uint8_t sendBuffor[MESSAGE_SIZE];
//     for (uint8_t i = 0; i < MESSAGE_SIZE; i++){
//         sendBuffor[i] = 0;
//     }
//     // // lambda function for setting message in sendBuffor
//     // auto setSendBuffor = [&](uint8_t* message) {
//     //     for (uint8_t i = 0; i < 6; i++){
//     //         sendBuffor[i] = message[i];
//     //     }
//     // };
    
//     xTimerStart(msAddressingTimeoutTimer, portMAX_DELAY);
//     for(;;) {
//         if (attemptCounter > ADDRESSING_MAX_ATTEMPTS) {
//             Serial.println("CONNECTION ERROR! In addressingTask() -> Exceeded max number of connection attempts");
//             // TODO add CONNECTION ERROR DebugLED blink
//             for (uint8_t i = 0; i < 6; i++){
//                 sendBuffor[i] = MESSAGES[3][i];
//             }
//             for (uint8_t i = 0; i < 3; i++) {
//                 xQueueSend(msSendMessagesQueue, &sendBuffor, portMAX_DELAY);
//                 vTaskDelay(pdMS_TO_TICKS(300));
//             }
//             deleteAddresingTimer();
//             mspDebugLED->deletePairingBlinkTask();
//             msAddressingTaskHandle = NULL;
//             vTaskDelay(pdMS_TO_TICKS(100));
//             vTaskDelete(NULL);
//         } else {
//             attemptCounter++;

//             switch (addresingStage) {
//                 case addresingStageEnum::newConnection:
//                     for (uint8_t i = 0; i < 6; i++){
//                         sendBuffor[i] = MESSAGES[0][i];
//                     }
//                     xTaskNotify(msReceiveMessageTaskHandle, mCommunicationMainNotifications::readRawMessage, eSetValueWithOverwrite);
//                     Serial.println("sending newcon");
//                     xQueueSend(msSendMessagesQueue, &sendBuffor, portMAX_DELAY);
//                     if (xQueueReceive(msReceiveMessageQueue, &receiveBuffor, pdMS_TO_TICKS(ADDRESSING_MESSAGE_TIMEOUT)) == pdTRUE) {
//                         Serial.println("xQueueReceive addresingStage");
//                         #ifdef RF_CHANNELS
//                             addresingStage = addresingStageEnum::newRFChannel;
//                         #else
//                             addresingStage = addresingStageEnum::summation;
//                         #endif
//                         attemptCounter = 0;
//                     }
//                     break;
                
//                 case addresingStageEnum::newRFChannel:
//                     Serial.println("addresingStageEnum::newRFChannel");
//                     Serial.print("last rec message: ");
//                     for (uint8_t i = 0; i < 16; i++) {
//                         Serial.print(receiveBuffor[i]);
//                         Serial.print(' ');
//                     }
//                     Serial.println();
//                     attemptCounter = 255;
//                     break;
                
//                 default:
//                     Serial.print("ADDRESING STAGE ERROR! In addressingTask() -> unknow addresingStage. AddresingStage: ");
//                     Serial.println(addresingStage);
//                     break;
//             }
//         }
//     }
// }
// #endif
void Communication::createAddressingTaskHandle(void *parameters) {
    Communication* instance = static_cast<Communication*>(parameters);
    instance->addressingTask();
}
void Communication::createAddressingTask() {
    createAddresingTimer();
    mspDebugLED->createPairingBlinkTask();
    // TODO remove print
    Serial.println("createAddressingTask()");
    if (msAddressingTaskHandle == NULL) {
        xTaskCreate(
            createAddressingTaskHandle,
            "Addressing Task",
            2048,
            NULL,
            MEDIUM_TASK_PRIORITY,
            &msAddressingTaskHandle
        );
    } else {
        Serial.println("TASK CREATION ERROR! In createAddressingTask() -> Can't create addressing task, because task already exists");
    }
}
void Communication::deleteAddressingTask() {
    deleteAddresingTimer();
    // TODO remove print
    Serial.println("deleteAddressingTask()");
    mspDebugLED->deletePairingBlinkTask();

    if (msAddressingTaskHandle != NULL) {
        vTaskDelete(msAddressingTaskHandle);
        msAddressingTaskHandle = NULL;
    }
}
// ================================================================

// ========================== setup HC12 ==========================

void Communication::setupHC12Task(void *parameters) {
    // TODO add changing baud rate of mspSerial if needed
    vTaskDelay(pdMS_TO_TICKS(50));
    mspSerial->write("AT");
    vTaskDelay(pdMS_TO_TICKS(50));
    xTaskNotify(msCommunicationMainTaskHandle, deleteSetupHC12TaskNotif, eSetValueWithOverwrite);

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
void Communication::createSetupHC12Task() {
    digitalWrite(SET_PIN, LOW);
    if (msSetupHC12TaskHandle == NULL) {
        xTaskCreate(
            setupHC12Task,
            "Setup HC12 Task",
            2048,
            NULL,
            MEDIUM_TASK_PRIORITY,
            &msSetupHC12TaskHandle
        );
    } else {
        Serial.println("TASK CREATION ERROR! In createSetupHC12Task() -> Can't create setup HC12 task, because task already exists");
    }
}
void Communication::deleteSetupHC12Task() {
    if (msSetupHC12TaskHandle != NULL) {
        vTaskDelete(msSetupHC12TaskHandle);
        msSetupHC12TaskHandle = NULL;
    }
    digitalWrite(SET_PIN, HIGH);
}
// ================================================================

// ============================ Timers ============================

void Communication::communicationTimersCallbacks(TimerHandle_t xTimer){
    if (xTimer == msReceiveMessageTimeoutTimer) {
        // TODO remove print
        Serial.println("message timeout callback");
        xTaskNotify(msReceiveMessageTaskHandle, messageTimeoutNotif, eSetValueWithOverwrite);
    } else if (xTimer == msReceiveByteTimeoutTimer) {
        // TODO remove print
        Serial.println("byte timeout callback");
        xTaskNotify(msCommunicationMainTaskHandle, byteTimeoutNotif, eSetValueWithOverwrite);
    } else if (xTimer == msSuspendReceiveMessageTimer) {
        xTaskNotify(msCommunicationMainTaskHandle, suspendReceiveMessageTaskNotif, eSetValueWithOverwrite);
    } else if (xTimer == msSuspendSendMessageTimer) {
        xTaskNotify(msCommunicationMainTaskHandle, suspendSendMessageTaskNotif, eSetValueWithOverwrite);
    }
}
void Communication::createCommunicationTimers() {
    if (msReceiveMessageTimeoutTimer == NULL) {
        msReceiveMessageTimeoutTimer = xTimerCreate(
            "Receive Message Timeout",
            pdMS_TO_TICKS(RECEIVE_MESSAGE_TIMEOUT),
            pdFALSE,
            NULL,
            communicationTimersCallbacks
        );
    }
    if (msReceiveByteTimeoutTimer == NULL) {
        msReceiveByteTimeoutTimer = xTimerCreate(
            "Receive Byte Timeout",
            pdMS_TO_TICKS(RECEIVE_BYTE_TIMEOUT),
            pdFALSE,
            NULL,
            communicationTimersCallbacks
        );
    }
    if (msSuspendReceiveMessageTimer == NULL) {
        msSuspendReceiveMessageTimer = xTimerCreate(
            "Suspend Receive Message",
            pdMS_TO_TICKS(SUSPEND_TASK_TIME),
            pdFALSE,
            NULL,
            communicationTimersCallbacks
        );
    }
    if (msSuspendSendMessageTimer == NULL) {
        msSuspendSendMessageTimer = xTimerCreate(
            "Suspend Send Message",
            pdMS_TO_TICKS(SUSPEND_TASK_TIME),
            pdFALSE,
            NULL,
            communicationTimersCallbacks
        );
    }
}
void Communication::deleteCommunicationTimers() {
    if (msReceiveMessageTimeoutTimer != NULL) {
        xTimerDelete(msReceiveMessageTimeoutTimer, portMAX_DELAY);
        msReceiveMessageTimeoutTimer = NULL;
    }
    if (msReceiveByteTimeoutTimer != NULL) {
        xTimerDelete(msReceiveByteTimeoutTimer, portMAX_DELAY);
        msReceiveByteTimeoutTimer = NULL;
    }
    if (msSuspendReceiveMessageTimer != NULL) {
        xTimerDelete(msSuspendReceiveMessageTimer, portMAX_DELAY);
        msSuspendReceiveMessageTimer = NULL;
    }
    if (msSuspendSendMessageTimer != NULL) {
        xTimerDelete(msSuspendSendMessageTimer, portMAX_DELAY);
        msSuspendSendMessageTimer = NULL;
    }
}


void Communication::addressingTimeoutTimerCallback(TimerHandle_t xTimer) {
    // TODO remove print
    Serial.println("Addressing Timeout Callback");
    xTaskNotify(msCommunicationMainTaskHandle, deleteAddressingTaskWithAbortNotif, eSetValueWithOverwrite);
}
void Communication::createAddresingTimer() {
    if (msAddressingTimeoutTimer == NULL) {
        msAddressingTimeoutTimer = xTimerCreate(
            "Addressing Timeout",
            pdMS_TO_TICKS(ADDRESSING_ABSOLUTE_TIMEOUT), 
            pdFALSE,
            NULL,
            addressingTimeoutTimerCallback
        );
    }
}
void Communication::deleteAddresingTimer() {
    if (msAddressingTimeoutTimer != NULL) {
        xTimerDelete(msAddressingTimeoutTimer, portMAX_DELAY);
        msAddressingTimeoutTimer = NULL;
    }
}
// ================================================================

// ============================ other =============================

void Communication::repeatMessage() {
    uint8_t sendBuffor[MESSAGE_SIZE];
    sendBuffor[0] = (uint8_t)'r';
    sendBuffor[1] = (uint8_t)'e';
    sendBuffor[2] = (uint8_t)'p';
    sendBuffor[3] = (uint8_t)'e';
    sendBuffor[4] = (uint8_t)'a';
    sendBuffor[5] = (uint8_t)'t';
    for (uint8_t i = 6; i < MESSAGE_SIZE; i++){
        sendBuffor[i] = 0;
    }

    // TODO remove print
    Serial.println("repeatMessage()");
    // TODO add delay before send message?
    // vTaskDelay(pdMS_TO_TICKS(200));
    xQueueSend(msSendMessagesQueue, &sendBuffor, portMAX_DELAY);
}

void Communication::setLastMessage(uint8_t *message, uint8_t size) {
    xSemaphoreTake(msLastMessageMutex, portMAX_DELAY);

    for (uint8_t i = 0; i < size; i++) {
        msLastMessage[i] = message[i];
    }

    uint8_t index = size;
    while (msLastMessage[index] != 0 && index < MESSAGE_SIZE) {
        msLastMessage[index] = 0;
        index++;
    }

    xSemaphoreGive(msLastMessageMutex);
}

void Communication::resetLastMessage() {
    xSemaphoreTake(msLastMessageMutex, portMAX_DELAY);

    uint8_t index = 0;
    while (msLastMessage[index] != 0 && index < MESSAGE_SIZE) {
        msLastMessage[index] = 0;
        index++;
    }

    xSemaphoreGive(msLastMessageMutex);
}

bool Communication::isRepeatMessage(uint8_t *message, uint8_t size) {
    const uint8_t repeat[] = {'r', 'e', 'p', 'e', 'a', 't'}; 

    if (size != 6) return false;
    for (uint8_t i = 0; i < 6; i++) {
        if (message[i] != repeat[i]) return false;
    }

    return true;
}
// ================================================================