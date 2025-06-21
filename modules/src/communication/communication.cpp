#include "communication/communication.h"
#include "smart_home_config.h"
#include <HardwareSerial.h>

#define MESSAGE_SIZE 64
#define PROTOCOL_MESSAGE_SIZE 16
#define BLANK_CHARACTER ' '
// TODO assign final value
#define RECEIVE_BYTE_TIMEOUT 100
// TODO assign final value
#define RECEIVE_MESSAGE_TIMEOUT 1000

uint8_t Communication::msMACAddress[6];
HardwareSerial* Communication::mspSerial = nullptr;
DebugLED* Communication::mspDebugLED = nullptr;


TaskHandle_t Communication::msPrintMessageTaskHandle = NULL;
TaskHandle_t Communication::msReceiveMessageTaskHandle = NULL;
TaskHandle_t Communication::msReadHC12HandlerTaskHandle = NULL;
// TODO remove "sendCustomMessage" methods
TaskHandle_t Communication::msSendCustomMessageTaskHandle = NULL;
TaskHandle_t Communication::msSendMessageTaskHandle = NULL;

QueueHandle_t Communication::msReceiveMessageQueue = NULL;
QueueHandle_t Communication::msReceiveByteQueue = NULL;
QueueHandle_t Communication::msSendMessagesQueue = NULL;

TimerHandle_t Communication::msReceiveMessageTimeoutTimer = NULL;
TimerHandle_t Communication::msReceiveByteTimeoutTimer = NULL;

Communication::Communication(DebugLED *debugLED) {
    // Get MAC address
    #ifdef ESP32_BOARD
        esp_read_mac(msMACAddress, ESP_MAC_WIFI_STA);
    #else
        // TODO add function to get MAC address on different boards
        #error "MAC address not implemented!"
    #endif 

    mspDebugLED = debugLED;

    pinMode(SET_PIN, OUTPUT);
    digitalWrite(SET_PIN, HIGH);
    
    mspSerial = new HardwareSerial(HARDWARE_SERIAL_UART_NR);
    mspSerial->begin((unsigned long)BAUD_RATE, SERIAL_8N1, RX_PIN, TX_PIN);

    createCommunicationQueues();

    createReadHC12HandlerTask();
    // TODO remove "sendCustomMessage" methods
    createSendCustomMessageTask();
    createSendMessageTask();

    createReceiveMessageTask();

//     createPrintMessageTask();
//     createSendCustomMessageTask();
    Serial.println("Communication initialized");
}

Communication::~Communication() {
    // TODO remove "sendCustomMessage" methods
    deleteSendCustomMessageTask();
    deleteSendMessageTask();

    deleteReadHC12HandlerTask();
    deleteReceiveMessageTask();
    // deletePrintMessageTask();

    deleteCommunicationQueues();

    deleteReceiveTimers();

    delete mspSerial;
    digitalWrite(SET_PIN, LOW);
}

void Communication::startAddresingAlgorithm() {
    Serial.println("startAddresingAlgorithm");
    mspDebugLED->createPairingBlinkTask();
}

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
    uint32_t timeoutStatus = mReadHC12NotificationStatus::defaultStatus;

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
    for (;;){
        xQueueReceive(msReceiveByteQueue, &queueBuffor, portMAX_DELAY);
        // Serial.println("xQueueReceive");
        // timeouts 
        if (xTaskNotifyWait(0, ULONG_MAX, &timeoutStatus, 0) == pdTRUE) {
            if (timeoutStatus == mReadHC12NotificationStatus::byteTimeout) {
                xTimerStop(msReceiveMessageTimeoutTimer, portMAX_DELAY);
                Serial.println("Byte timeout");
            } else if (timeoutStatus == mReadHC12NotificationStatus::messageTimeout) {
                xTimerStop(msReceiveByteTimeoutTimer, portMAX_DELAY);
                Serial.println("Message timeout");
            } else {
                Serial.print("STATUS ERROR! In receiveMessageTask() -> got unknow status. Received Status: ");
                Serial.println(timeoutStatus);
            }
            resetProtocolBuffor();
            xQueueReset(msReceiveByteQueue);
            timeoutStatus = mReadHC12NotificationStatus::defaultStatus;
        } 
        // decoding message
        else {
            protocolBuffor[pbMessageIndex][pbByteIndex] = queueBuffor;
            // if new message start message timeout timer
            if (pbByteIndex == 0 && pbMessageIndex == 0){
                xTimerStart(msReceiveMessageTimeoutTimer, portMAX_DELAY);
            }
            // if protocol message is not complete
            if (pbByteIndex != PROTOCOL_MESSAGE_SIZE - 1) {
                pbByteIndex++;
                // xTimerStart(msReceiveByteTimeoutTimer, portMAX_DELAY);
            } else {
                xTimerStop(msReceiveByteTimeoutTimer, portMAX_DELAY);
                pbByteIndex = 0;

                // if message is not end properly
                if (protocolBuffor[pbMessageIndex][PROTOCOL_MESSAGE_SIZE - 1] != 0) {
                    xTimerStop(msReceiveMessageTimeoutTimer, portMAX_DELAY);
                    
                    // TODO add "repeat last message"
                    Serial.print("BAD END OF MESSAGE ERROR! In receiveMessageTask() -> message should end with 0 (\\0 char), but got: ");
                    Serial.println(protocolBuffor[pbMessageIndex][PROTOCOL_MESSAGE_SIZE - 1]);

                    resetProtocolBuffor();
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
                        resetProtocolBuffor();
                        // TODO add "repeat last message"
                        Serial.println("BAD CHECKSUM ERROR! In receiveMessageTask() -> checksum incorrect");
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
                            }

                            messagesQuantity = protocolBuffor[pbMessageIndex][7];
                            pbMessageIndex++;
                        } while(messagesQuantity != 0);

                        // TODO add message to queue
                        Serial.print("Received message: ");
                        Serial.println((char*)messageBuffor);

                        // clean up
                        resetProtocolBuffor();
                    }
                }
            }
        }

        // reading message 
        // else if (mspSerial->available() && xSemaphoreTake(msHC12ReceiveMutex, 0) == pdTRUE) {
            
        
            
        // } 
        // idle
        // else {
        //     // delay for watchdog
        //     vTaskDelay(pdMS_TO_TICKS(1));
        // }
    }
}
void Communication::createReceiveMessageTaskHandle(void *parameters) {
    Communication* instance = static_cast<Communication*>(parameters);
    instance->receiveMessageTask();
}
void Communication::createReceiveMessageTask() {
    createReceiveTimers();

    if (msReceiveMessageTaskHandle == NULL) {
        xTaskCreate(
            createReceiveMessageTaskHandle,
            "Receive message",
            2048,
            NULL,
            1,
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

    deleteReceiveTimers();
}
// ================================================================

// ====================== Read HC12 Handler =======================

void Communication::readHC12HandlerTask() {
    uint32_t status = mReadHC12NotificationStatus::defaultStatus;

    bool isSendingTaskWaiting = false;
    uint8_t hc12Input;
    
    
    for(;;) {
        // change status
        xTaskNotifyWait(0, ULONG_MAX, &status, 0);

        switch (status) {
            case mReadHC12NotificationStatus::defaultStatus:
                if (mspSerial->available()) {
                    // Serial.print("defaultStatus: ");
                    // Serial.println(isSendingTaskWaiting);

                    if (isSendingTaskWaiting) {
                        xTimerStop(msReceiveByteTimeoutTimer, portMAX_DELAY);
                        hc12Input = mspSerial->read();
                        xTaskNotify(msSendMessageTaskHandle, (uint32_t)hc12Input, eSetValueWithOverwrite);
                        isSendingTaskWaiting = false;
                    } else {
                        hc12Input = mspSerial->read();
                        xQueueSend(msReceiveByteQueue, &hc12Input, portMAX_DELAY);
                        xTimerStart(msReceiveByteTimeoutTimer, portMAX_DELAY);
                    }
                }
                break;

            case mReadHC12NotificationStatus::byteTimeout:
                if (isSendingTaskWaiting) {
                    // max value that hc12 can send is 255, so notifying Send Message Task with 256 value mean timeout
                    Serial.println("xTaskNotify");
                    xTaskNotify(msSendMessageTaskHandle, 256, eSetValueWithOverwrite);
                    isSendingTaskWaiting = false;
                    status = mReadHC12NotificationStatus::defaultStatus;
                } else {
                    xTaskNotify(msReceiveMessageTaskHandle, mReadHC12NotificationStatus::byteTimeout, eSetValueWithOverwrite);
                    // Sending queue to "awake" task
                    xQueueSend(msReceiveByteQueue, &hc12Input, portMAX_DELAY);
                    status = mReadHC12NotificationStatus::defaultStatus;
                }
                break;

            case mReadHC12NotificationStatus::sendingTaskWaiting: 
                if (!isSendingTaskWaiting) {
                    isSendingTaskWaiting = true;
                    xTimerStart(msReceiveByteTimeoutTimer, portMAX_DELAY);
                    status = mReadHC12NotificationStatus::defaultStatus;
                }
                break;

            default:
                Serial.print("STATUS ERROR! In readHC12HandlerTask() -> got unknow status. Received Status: ");
                Serial.println(status);
                isSendingTaskWaiting = false;
                status = mReadHC12NotificationStatus::defaultStatus;
        }

        // delay for watchdog
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
void Communication::createReadHC12HandlerTaskHandle(void *parameters) {
    Communication* instance = static_cast<Communication*>(parameters);
    instance->readHC12HandlerTask();
}
void Communication::createReadHC12HandlerTask() {
    if (msReadHC12HandlerTaskHandle == NULL) {
        xTaskCreate(
            createReadHC12HandlerTaskHandle,
            "Read HC12 Handler",
            2048,
            NULL,
            1,
            &msReadHC12HandlerTaskHandle
        );
    } else {
        Serial.println("TASK CREATION ERROR! In createReadHC12HandlerTask() -> Can't create Read HC12 Handler task, because task already exists");
    }
}
void Communication::deleteReadHC12HandlerTask() {
    if (msReadHC12HandlerTaskHandle != NULL) {
        vTaskDelete(msReadHC12HandlerTaskHandle);
        msReadHC12HandlerTaskHandle = NULL;
    }
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
                
                // debug print
                uint8_t i = 0;
                Serial.print("Message Ready: ");
                while (buffor[i] != 0) {
                    Serial.print(buffor[i]);
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
            2,
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

        // TODO change to deleting itself after not receiving message for some time
        // wait until the message appears in the queue and save message in local messageBuffor
        xQueueReceive(msSendMessagesQueue, &messageBuffor, portMAX_DELAY);

        // divide and add messages to protocolBuffor
        while(messageIndex < 64 && messageBuffor[messageIndex] != 0) {
            protocolBuffor[messagesQuantity][(messageIndex % 6) + 8] = messageBuffor[messageIndex];
            messageIndex++;
            if (messageIndex % 6 == 0) {
                messagesQuantity++;
            }
        }
        messagesQuantity++;

        // TODO remove 
        for (uint8_t i = 0; i < 11; i++){
            Serial.print("message in protocolBuffor: ");
            for (uint8_t j = 8; j < 14; j++){
                Serial.print((char)protocolBuffor[i][j]);
            }
            Serial.println();
        }

        // add messagesQuantity and checksum to protocolBuffor
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
        // TODO remove TEST
        Serial.println("Messages to send:");
        for (uint8_t i = 0; i < 11; i++){
            Serial.print("Message ");
            Serial.print(i);
            Serial.print(": ");
            for (uint8_t j = 0; j < 16; j++){
                Serial.print((int)protocolBuffor[i][j]);
                Serial.print(" ");
            }
            Serial.println();
        }

        // send message and clean buffor
        i = 0;
        do {
            Serial.println("sending..."); 
            xTaskNotify(msReadHC12HandlerTaskHandle, mReadHC12NotificationStatus::sendingTaskWaiting, eSetValueWithOverwrite);
            mspSerial->write(protocolBuffor[i], PROTOCOL_MESSAGE_SIZE);
            
            // wait until hc12 module send confirmation
            uint32_t hc12Respond;
            xTaskNotifyWait(0, ULONG_MAX, &hc12Respond, portMAX_DELAY);
            if (hc12Respond == 256){
                Serial.println("SENDING MESSAGE ERROR! In sendMessageTask() -> hc12 module is not responding.");
            } else if (hc12Respond != 255) {
                Serial.print("SENDING MESSAGE ERROR! In sendMessageTask() -> hc12 module did not confirm properly. Hc-12 module should send 255 signal but got: ");
                Serial.println(hc12Respond);
            }

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
            3,
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

void Communication::addressingTask() {

}
void Communication::createAddressingTaskHandle(void *parameters) {

}
void Communication::createAddressingTask() {

}
void Communication::deleteAddressingTask() {

}
// ================================================================

// TODO remove printMessageTask

// ============================= test =============================
void Communication::printMessageTask() {
    char receivedData[MESSAGE_SIZE];
    for(;;) {
        if (xQueueReceive(msReceiveMessageQueue, &receivedData, portMAX_DELAY) == pdTRUE) {
            Serial.print("xQueueReceive: " + String(receivedData));
        }
    }
}
void Communication::createPrintMessageTaskHandle(void *parameters) {
    Communication* instance = static_cast<Communication*>(parameters);
    instance->printMessageTask();
}
void Communication::createPrintMessageTask() {
    if (msPrintMessageTaskHandle == NULL) {
        xTaskCreate(
            createPrintMessageTaskHandle,
            "Print message",
            2048,
            NULL,
            1,
            &msPrintMessageTaskHandle
        );
    } else {
        Serial.println("TASK CREATION ERROR! In createPrintMessageTask() -> Can't create print message task, because task already exists");
    }
}
void Communication::deletePrintMessageTask() {
    if (msPrintMessageTaskHandle != NULL) {
        vTaskDelete(msPrintMessageTaskHandle);
        msPrintMessageTaskHandle = NULL;
    }
}
// ================================================================

// ============================ Timers ============================

void Communication::receiveMessageTimeoutTimerCallback() {
    xTaskNotify(msReceiveMessageTaskHandle, mReadHC12NotificationStatus::messageTimeout, eSetValueWithOverwrite);
    // Sending 0 to queue to "awake" task
    uint8_t value = 0;
    xQueueSend(msReceiveByteQueue, &value, portMAX_DELAY);
}
void Communication::receiveMessageTimeoutTimerCallbackHandle(TimerHandle_t xTimer) {
    Communication* instance = static_cast<Communication*>(pvTimerGetTimerID(xTimer));
    instance->receiveMessageTimeoutTimerCallback();
}
void Communication::receiveByteTimeoutTimerCallback() {
    xTaskNotify(msReadHC12HandlerTaskHandle, mReadHC12NotificationStatus::byteTimeout, eSetValueWithOverwrite);
}
void Communication::receiveByteTimeoutTimerCallbackHandle(TimerHandle_t xTimer) {
    Communication* instance = static_cast<Communication*>(pvTimerGetTimerID(xTimer));
    instance->receiveByteTimeoutTimerCallback();
}
void Communication::createReceiveTimers() {
    if (msReceiveMessageTimeoutTimer == NULL) {
        msReceiveMessageTimeoutTimer = xTimerCreate(
            "Receive Message Timeout",
            pdMS_TO_TICKS(RECEIVE_MESSAGE_TIMEOUT),
            pdFALSE,
            NULL,
            receiveMessageTimeoutTimerCallbackHandle
        );
    }
    if (msReceiveByteTimeoutTimer == NULL) {
        msReceiveByteTimeoutTimer = xTimerCreate(
            "Receive Char Timeout",
            pdMS_TO_TICKS(RECEIVE_BYTE_TIMEOUT),
            pdFALSE,
            NULL,
            receiveByteTimeoutTimerCallbackHandle
        );
    }
}
void Communication::deleteReceiveTimers() {
    if (msReceiveMessageTimeoutTimer != NULL) {
        xTimerDelete(msReceiveMessageTimeoutTimer, portMAX_DELAY);
    }
    if (msReceiveByteTimeoutTimer != NULL) {
        xTimerDelete(msReceiveByteTimeoutTimer, portMAX_DELAY);
    }
}
// ================================================================