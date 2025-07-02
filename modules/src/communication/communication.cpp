// TODO !BEFORE PULL REQUEST! check for "eSetValueWithoutOverwrite"!
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

portMUX_TYPE Communication::msCriticalSectionMutex = portMUX_INITIALIZER_UNLOCKED;

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

TimerHandle_t Communication::msAddressingAbsoluteTimeoutTimer = NULL;

#ifdef CENTRAL_UNIT
    Communication::Routing Communication::msRoutingTable[255];
#else
    uint8_t Communication::msCentralUnitMACAddress[6] = {0, 0, 0, 0, 0, 0};
    uint8_t Communication::msIPAddress = 0;
    SemaphoreHandle_t Communication::msAddressDataMutex = NULL;
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
    #else
        msAddressDataMutex = xSemaphoreCreateMutex();
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
    deleteAddresingTimers();

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
                    xTaskNotify(msReceiveMessageTaskHandle, byteTimeoutNotif, eSetValueWithOverwrite);
                }
                break;

            case messageTimeoutNotif:
                xTaskNotify(msReceiveMessageTaskHandle, messageTimeoutNotif, eSetValueWithOverwrite);
                break;

            case readRawMessageNotif:
                xTaskNotify(msReceiveMessageTaskHandle, readRawMessageNotif, eSetValueWithOverwrite);
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

            // case addressingMessageTimeoutNotif:
            //     xTaskNotify(msAddressingTaskHandle, addressingMessageTimeoutNotif, eSetValueWithOverwrite);
            //     break;

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

    // buffor for byte received from HC12
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
                    // TODO add check for MAC and IP addresses !!! REMEMBER ABOUT MUTEX - msAddressDataMutex !!!
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

                        // TODO remove print
                        #ifdef CENTRAL_UNIT
                            Serial.println("CENTRAL_UNIT received: ");
                            for (uint8_t i = 0; i < 16; i++) {
                                Serial.print(protocolBuffor[0][i]);
                                Serial.print(' ');
                            }
                            Serial.println();
                        #endif

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

        // prepare IP and MAC addresses
        #ifdef CENTRAL_UNIT
            for (uint8_t j = 0; j < 6; j++){
                protocolBuffor[i][j] = msMACAddress[j];
            }
            protocolBuffor[i][6] = 1;
        #else
            xSemaphoreTake(msAddressDataMutex, portMAX_DELAY);
            protocolBuffor[i][6] = msIPAddress;
            if (msIPAddress == 0) {
                for (uint8_t j = 0; j < 6; j++){
                    protocolBuffor[i][j] = msMACAddress[j];
                }
            } else {
                for (uint8_t j = 0; j < 6; j++){
                    protocolBuffor[i][j] = msCentralUnitMACAddress[j];
                }
            }
            xSemaphoreGive(msAddressDataMutex);
        #endif

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
                // TODO remove print
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
#ifdef CENTRAL_UNIT
// TODO add abortAddressing() and addressingTask()
void Communication::abortAddressing() {}
void Communication::addressingTask() {
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
#else
void Communication::abortAddressing() {
    // TODO add cleanup
    // cleanup tmp addresing global (class) variables
    xSemaphoreTake(msAddressDataMutex, portMAX_DELAY);
    for (uint8_t i = 0; i < 6; i++) {
        msCentralUnitMACAddress[i] = 0;
    }
    msIPAddress = 0;
    xSemaphoreGive(msAddressDataMutex);
    // reset sendMessageTask
    taskENTER_CRITICAL(&msCriticalSectionMutex);
    deleteSendMessageTask();
    createSendMessageTask();
    taskEXIT_CRITICAL(&msCriticalSectionMutex);

    // prepare message to send
    uint8_t sendBuffor[MESSAGE_SIZE];
    sendBuffor[0] = (uint8_t)'a';
    sendBuffor[1] = (uint8_t)'b';
    sendBuffor[2] = (uint8_t)'o';
    sendBuffor[3] = (uint8_t)'r';
    sendBuffor[4] = (uint8_t)'t';
    for (uint8_t i = 5; i < MESSAGE_SIZE; i++){
        sendBuffor[i] = 0;
    }

    // send "abort" message 3 times
    // TODO remove print
    Serial.println("abortAddressing()");
    for (uint8_t i = 0; i < 3; i++) {
        xQueueSend(msSendMessagesQueue, &sendBuffor, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    // Notify - delete Addressing Task
    xTaskNotify(msCommunicationMainTaskHandle, deleteAddressingTaskNotif, eSetValueWithOverwrite);
}

void Communication::addressingTask() {
    // start absolute timeout timer
    xTimerStart(msAddressingAbsoluteTimeoutTimer, portMAX_DELAY);
    // vTaskDelay(pdMS_TO_TICKS(6000));
    // xTaskNotify(msCommunicationMainTaskHandle, deleteAddressingTaskNotif, eSetValueWithOverwrite);

    enum ADDRESSING_STAGES : uint8_t {
        newConnectionRequest = 0,
        changeRadioChannel,
        waitForPing,
        summary,
        waitForDeletion
    };
    uint8_t addressingStage = newConnectionRequest;

    // TODO add message for new connection with real MAC address and without it
    // 0  new connection - request for new connection
    // 1  ping - ping
    // 2  reply ping - replay to ping
    // 3  abort - abort new connection
    // 4  summary - after this message central unit will send all data about connection for verification 
    // 5  ok summary - confirmation for data sent in summary
    // 6  wrong summary - rejection for data sent in summary
    // 7  ip [number] - reply for "new connection" with new ip for module. WARNING! [number] have to be set separately!!! 
    // 8  yes rf channels, yes transmission (without) permission - can change rf channels,   want to       transmit without permission
    // 9  yes rf channels, no  transmission (without) permission - can change rf channels,   don't want to transmit without permission
    // 10 no  rf channels, yes transmission (without) permission - can't change rf channels, want to       transmit without permission
    // 11 no  rf channels, no  transmission (without) permission - can't change rf channels, don't want to transmit without permission
    const uint8_t MESSAGES[][6] = {
        {'n', 'e', 'w', 'c', 'o', 'n'}, 
        {'p', 'i', 'n', 'g', '\0', '\0'}, 
        {'r', 'e', 'p', 'i', 'n', 'g'}, 
        {'a', 'b', 'o', 'r', 't', '\0'},
        {'s', 'u', 'm', 'm', 'a', 'r'},
        {'o', 'k', 's', 'u', 'm', 'm'},
        {'w', 'r', 's', 'u', 'm', 'm'},
        {'i', 'p', '\0', '\0', '\0', '\0'},
        {'y', 'r', 'c', 'y', 't', 'p'}, 
        {'y', 'r', 'c', 'n', 't', 'p'}, 
        {'n', 'r', 'c', 'y', 't', 'p'}, 
        {'n', 'r', 'c', 'n', 't', 'p'}, 
    };

    // prepare buffors
    uint8_t receiveBuffor[MESSAGE_SIZE];
    // lambda function for clearing buffor
    auto resetReceiveBuffor = [&]() {
        for (uint8_t i = 0; i < MESSAGE_SIZE; i++){
            receiveBuffor[i] = 0;
        }
    };
    resetReceiveBuffor();

    uint8_t lastReceivedMessage[6]; 
    for (uint8_t i = 0; i < 6; i++){
        lastReceivedMessage[i] = 0;
    }

    uint8_t sendBuffor[MESSAGE_SIZE];
    // lambda function for clearing buffor
    auto resetSendBuffor = [&]() {
        for (uint8_t i = 0; i < MESSAGE_SIZE; i++){
            sendBuffor[i] = 0;
        }
    };
    resetSendBuffor();


    uint8_t attemptsCounter = 0;
    for (;;) {
        attemptsCounter++;
        if (attemptsCounter > ADDRESSING_MAX_ATTEMPTS) {
            xTimerStop(msAddressingAbsoluteTimeoutTimer, portMAX_DELAY);
            Serial.println("ADDRESSING ERROR! In addressingTask() -> exceeded max number of new connection attempts.");
            xTaskNotify(msCommunicationMainTaskHandle, deleteAddressingTaskWithAbortNotif, eSetValueWithOverwrite);
            addressingStage = waitForDeletion;
        }

        switch (addressingStage) {
            case newConnectionRequest:
                // send "newcon" message
                for (uint8_t i = 0; i < 6; i++) {
                    // MESSAGES[0] = "newcon"
                    sendBuffor[i] = MESSAGES[0][i];
                }
                xTaskNotify(msCommunicationMainTaskHandle, readRawMessageNotif, eSetValueWithOverwrite);
                xQueueSend(msSendMessagesQueue, &sendBuffor, portMAX_DELAY);
                resetSendBuffor();

                // wait for response from central unit
                if (xQueueReceive(msReceiveMessageQueue, receiveBuffor, pdMS_TO_TICKS(RECEIVE_MESSAGE_TIMEOUT)) == pdTRUE) {
                    // TODO add check is good message
                    // TODO add saving last received message

                    // set "IP" address and central unit's MAC address
                    /* raw message: [0-5{mac}, 6{ip}, 7{messagesQuantity}, 8-13{message}, 14{checksum}, 15{\0}] =>
                       central unit should respond {mac1}{mac2}{mac3}{mac4}{mac5}{mac6}{'1'}{'0'}{'i'}{'p'}{new "IP" address}... =>
                       new "IP" - receiveBuffor[10] */
                    xSemaphoreTake(msAddressDataMutex, portMAX_DELAY);
                    for (uint8_t i = 0; i < 6; i++) {
                        msCentralUnitMACAddress[i] = receiveBuffor[i];
                    }
                    msIPAddress = receiveBuffor[10];
                    xSemaphoreGive(msAddressDataMutex);

                    // reset sendMessageTask
                    taskENTER_CRITICAL(&msCriticalSectionMutex);
                    deleteSendMessageTask();
                    createSendMessageTask();
                    taskEXIT_CRITICAL(&msCriticalSectionMutex);

                    // TODO remove print
                    Serial.print("respond from central unit: ");
                    for (uint8_t i = 0; i < 16; i++) {
                        Serial.print(receiveBuffor[i]);
                        Serial.print(' ');
                    }
                    Serial.println();

                    // go to next stage
                    #ifdef RF_CHANNELS
                        addressingStage = changeRadioChannel;
                    #else
                        #error "ifndef RF_CHANNELS not implemented!"
                    #endif
                    resetReceiveBuffor();
                    attemptsCounter = 0;
                } 
                // TODO remove print
                else {
                    Serial.println("NEW CONNECTION ERROR! In addressingTask() -> central unit is not responding.");
                }
                break;

            case changeRadioChannel:
                vTaskDelay(pdMS_TO_TICKS(50));
                #ifdef TRANSMISSION_WITHOUT_PERMISSION
                    for (uint8_t i = 0; i < 6; i++) {
                        // MESSAGES[8] = "yrcytp"
                        sendBuffor[8] = MESSAGES[8][i];
                    }
                #else
                    for (uint8_t i = 0; i < 6; i++) {
                        // MESSAGES[9] = "yrcntp"
                        sendBuffor[9] = MESSAGES[9][i];
                    }
                #endif
                xQueueSend(msSendMessagesQueue, sendBuffor, portMAX_DELAY);
                resetSendBuffor();

                Serial.println("add changeRadioChannel...");
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;

            case waitForDeletion: 
                for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
                break;

            default:
                Serial.print("STATUS ERROR! In communicationMainTask() -> got unknow addressingStage. Received addressingStage: ");
                Serial.println(addressingStage);
                break;
        }
    }
}
#endif
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
void Communication::createAddressingTaskHandle(void *parameters) {
    Communication* instance = static_cast<Communication*>(parameters);
    instance->addressingTask();
}
void Communication::createAddressingTask() {
    createAddresingTimers();
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
    deleteAddresingTimers();
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


void Communication::addressingTimersCallbacks(TimerHandle_t xTimer) {
    if (xTimer == msAddressingAbsoluteTimeoutTimer) {
        xTaskNotify(msCommunicationMainTaskHandle, deleteAddressingTaskWithAbortNotif, eSetValueWithOverwrite);
    } 
    // else if (xTimer == msAddressingMessageTimeoutTimer) {
    //     xTaskNotify(msCommunicationMainTaskHandle, addressingMessageTimeoutNotif, eSetValueWithOverwrite);
    // }
}
void Communication::createAddresingTimers() {
    if (msAddressingAbsoluteTimeoutTimer == NULL) {
        msAddressingAbsoluteTimeoutTimer = xTimerCreate(
            "Addressing Absolute Timeout",
            pdMS_TO_TICKS(ADDRESSING_ABSOLUTE_TIMEOUT), 
            pdFALSE,
            NULL,
            addressingTimersCallbacks
        );
    }
    // if (msAddressingMessageTimeoutTimer == NULL) {
    //     msAddressingMessageTimeoutTimer = xTimerCreate(
    //         "Addressing Message Timeout",
    //         pdMS_TO_TICKS(ADDRESSING_ABSOLUTE_TIMEOUT), 
    //         pdFALSE,
    //         NULL,
    //         addressingTimersCallbacks
    //     );
    // }
}
void Communication::deleteAddresingTimers() {
    if (msAddressingAbsoluteTimeoutTimer != NULL) {
        xTimerDelete(msAddressingAbsoluteTimeoutTimer, portMAX_DELAY);
        msAddressingAbsoluteTimeoutTimer = NULL;
    }
    // if (msAddressingMessageTimeoutTimer != NULL) {
    //     xTimerDelete(msAddressingMessageTimeoutTimer, portMAX_DELAY);
    //     msAddressingMessageTimeoutTimer = NULL;
    // }
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