#include "communication/communication.h"
#include "smart_home_config.h"
#include <HardwareSerial.h>

#define MESSAGE_SIZE 64
#define PROTOCOL_MESSAGE_SIZE 16
#define BLANK_CHARACTER ' '

uint8_t Communication::msMACAddress[6];

HardwareSerial* Communication::mspSerial = nullptr;

TaskHandle_t Communication::msReceiveMessageTaskHandle = NULL;
TaskHandle_t Communication::msPrintMessageTaskHandle = NULL;
TaskHandle_t Communication::msSendMessageTaskHandle = NULL;

// TODO remove "sendCustomMessage" methods
TaskHandle_t Communication::msSendCustomMessageTaskHandle = NULL;

QueueHandle_t Communication::msReceiveMessageQueue = NULL;
QueueHandle_t Communication::msSendMessagesQueue = NULL;

SemaphoreHandle_t Communication::msHC12ReceiveMutex;

Communication::Communication() {
    // Get MAC address
    #ifdef ESP32_BOARD
        esp_read_mac(msMACAddress, ESP_MAC_WIFI_STA);
    #else
        // TODO add function to get MAC address on different boards
        #error "MAC address not implemented!"
    #endif 

    pinMode(SET_PIN, OUTPUT);
    digitalWrite(SET_PIN, HIGH);
    // TODO check HC12 print random characters
    // without this delay HC12 may print random characters if immediately enters "command mode"
    // vTaskDelay(pdMS_TO_TICKS(100));
    // digitalWrite(SET_PIN, LOW);
    // vTaskDelay(pdMS_TO_TICKS(100));
    // digitalWrite(SET_PIN, HIGH);
    // vTaskDelay(pdMS_TO_TICKS(100));

    msHC12ReceiveMutex = xSemaphoreCreateMutex();
    
    mspSerial = new HardwareSerial(HARDWARE_SERIAL_UART_NR);
    mspSerial->begin((unsigned long)BAUD_RATE, SERIAL_8N1, RX_PIN, TX_PIN);

    createReceiveMessageQueue();
    createSendMessageQueue();

    // TODO remove "sendCustomMessage" methods
    createSendCustomMessageTask();
    createSendMessageTask();

    createReceiveMessageTask();

//     createPrintMessageTask();
//     createSendCustomMessageTask();
    Serial.println("Communication initialized");
}

Communication::~Communication() {
    delete mspSerial;
    mspSerial = nullptr;

    // TODO remove "sendCustomMessage" methods
    deleteSendCustomMessageTask();
    deleteSendMessageTask();

    deleteReceiveMessageTask();

    deleteReceiveMessageQueue();
    deleteSendMessageQueue();

    digitalWrite(SET_PIN, LOW);
}

// ============================ Queues ============================

void Communication::createReceiveMessageQueue() {
    if (msReceiveMessageQueue == NULL) {
        msReceiveMessageQueue = xQueueCreate(10, sizeof(uint8_t[MESSAGE_SIZE]));
    }
}
void Communication::deleteReceiveMessageQueue() {
    if (msReceiveMessageQueue != NULL) {
        vQueueDelete(msReceiveMessageQueue);
        msReceiveMessageQueue = NULL;
    }
}
void Communication::createSendMessageQueue() {
    if (msSendMessagesQueue == NULL) {
        msSendMessagesQueue = xQueueCreate(10, sizeof(uint8_t[MESSAGE_SIZE]));
    }
}
void Communication::deleteSendMessageQueue() {
    if (msSendMessagesQueue != NULL) {
        vQueueDelete(msSendMessagesQueue);
        msSendMessagesQueue = NULL;
    }
}
// ================================================================

// ======================= Receive Message ========================

void Communication::receiveMessageTask() {
    uint8_t message;
    for (;;){
        if (mspSerial->available()){
            if (xSemaphoreTake(msHC12ReceiveMutex, 0) == pdTRUE){
                message = mspSerial->read();
                xSemaphoreGive(msHC12ReceiveMutex);
                if (message == 0) {
                    Serial.println();
                } else {
                    Serial.print(message);
                    Serial.print(' ');
                }
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }

    // prepare message protocol buffor
    // [0-5{mac}, 6{ip}, 7{messagesQuantity}, 8-13{message}, 14{checksum}, 15{\0}]
    // uint8_t protocolBuffor[11][PROTOCOL_MESSAGE_SIZE];
    // for (uint8_t i = 0; i < 11; i++){
    //     for (uint8_t j = 0; j < PROTOCOL_MESSAGE_SIZE; j++) {
    //         protocolBuffor[i][j] = 0;
    //     }
    // }

    // // prepare message to send buffor
    // uint8_t messageBuffor[MESSAGE_SIZE];
    // for (uint8_t i = 0; i < MESSAGE_SIZE; i++){
    //     messageBuffor[i] = 0;
    // }

    // for (;;) {
    //     if (mspSerial->available()) {

    //     }

    //     // delay for watchdog
    //     vTaskDelay(10);
    // }



    // TODO remove
    // uint8_t buffer[MESSAGE_SIZE];
    // for (uint8_t i = 0; i < MESSAGE_SIZE; i++) {
    //     buffer[i] = 0;
    // }
    // uint8_t i = 0;
    // bool isMessageComplete = false;

    // for(;;) {
    //     if (mspSerial->available()) {
    //         buffer[i] = mspSerial->read();
    //         Serial.print("i: ");
    //         Serial.println(i);
    //         if (i >= 16) {                
    //             isMessageComplete = true;
    //         }
    //         // TODO do something with getting 255 message from hc12
    //         if ((int)buffer[i] != 255) {
    //             i++;
    //         }
    //     }

    //     if (isMessageComplete) {
    //         isMessageComplete = false;
    //         Serial.print("Received: " + String(buffer));
    //         // Serial.print("Received (int): ");
    //         // vTaskDelay(pdMS_TO_TICKS(100));
    //         // mspSerial->write(buffer);
    //         // for (int j = 0; j < i; j++) {
    //         //     Serial.print(intBuffer[j]);
    //         // }
    //         // Serial.println("\n-----------");
    //         // TODO remove "msReceiveMessageBuffer"
    //         // xMessageBufferSend(msReceiveMessageBuffer, &buffer, sizeof(buffer), portMAX_DELAY);
    //         // xQueueSend(msReceiveMessageQueue, &buffer, portMAX_DELAY);

    //         for (int j = 0; j <= i; j++) {
    //             buffer[j] = '\0';
    //         }
    //         i = 0;
    //     }
    //     // watchdog delay
    //     vTaskDelay(pdMS_TO_TICKS(10));
    // }
}
void Communication::createReceiveMessageTaskHandle(void *parameters) {
    Communication* instance = static_cast<Communication*>(parameters);
    instance->receiveMessageTask();
}
void Communication::createReceiveMessageTask() {
    if (msReceiveMessageTaskHandle == NULL) {
        xTaskCreate(
            createReceiveMessageTaskHandle,
            "Receive message",
            2048,
            NULL,
            0,
            &msReceiveMessageTaskHandle
        );
    } else {
        Serial.println("void createReceiveMessageTask() - Can't create receive message task -> receive message task already exists");
    }
}
void Communication::deleteReceiveMessageTask() {
    if (msReceiveMessageTaskHandle != NULL) {
        vTaskDelete(msReceiveMessageTaskHandle);
        msReceiveMessageTaskHandle = NULL;
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
            1,
            &msSendCustomMessageTaskHandle
        );
    } else {
        Serial.println("void createSendCustomMessageTask() - Can't create send custom message task -> send custom message task already exists");
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
        for (uint8_t j = 0; j < 6; j++){
            protocolBuffor[i][j] = msMACAddress[j];
        }
        // TODO change to IP address
        protocolBuffor[i][6] = 255;
        for (uint8_t j = 7; j < PROTOCOL_MESSAGE_SIZE; j++) {
            protocolBuffor[i][j] = 0;
        }
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
        while(messageBuffor[messageIndex] != 0) {
            Serial.print("message in protocolBuffor: ");
            for (uint8_t i = 0; i < 6; i++){
                Serial.print((char)messageBuffor[messageIndex + i]);
                protocolBuffor[messagesQuantity][i + 8] = messageBuffor[messageIndex + i];
            }
            Serial.println();
            messagesQuantity++;
            messageIndex += 6;
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
            xSemaphoreTake(msHC12ReceiveMutex, portMAX_DELAY);
            mspSerial->write(protocolBuffor[i], PROTOCOL_MESSAGE_SIZE);
            
            // wait until hc12 module send confirmation 
            bool isHC12confirmed = false;
            while (!isHC12confirmed) {
                if (mspSerial->available()) {
                    uint8_t hc12confirmation = mspSerial->read();
                    xSemaphoreGive(msHC12ReceiveMutex);
                    if (hc12confirmation != 255) {
                        Serial.print("SENDING MESSAGE ERROR! In sendMessageTask() -> hc-12 module not confirmed properly. Hc-12 module should send 255 signal but got: ");
                        Serial.println(hc12confirmation);
                    }
                    isHC12confirmed = true;
                } 
                
                vTaskDelay(pdMS_TO_TICKS(1));
            }

            messagesQuantity = protocolBuffor[i][7];
            for (uint8_t j = 7; j < PROTOCOL_MESSAGE_SIZE; j++) {
                protocolBuffor[i][j] = 0;
            }
            
            i++;
            // this delay is required for HC12 send/receive properly message
            // TODO increase delay?
            vTaskDelay(pdMS_TO_TICKS(10));
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
        Serial.println("void createSendMessageTask() - Can't create send message task -> send message task already exists");
    }
}
void Communication::deleteSendMessageTask() {
    if (msSendMessageTaskHandle != NULL) {
        vTaskDelete(msSendMessageTaskHandle);
        msSendMessageTaskHandle = NULL;
    }
}
// ================================================================

// TODO remove printMessageTask

// ============================= test =============================
void Communication::printMessageTask() {
    // char receivedData[MESSAGE_SIZE];
    // for(;;) {
    //     if (xQueueReceive(msReceiveMessageQueue, &receivedData, portMAX_DELAY) == pdTRUE) {
    //         Serial.print("xQueueReceive: " + String(receivedData));
    //     }
    // }
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
        Serial.println("void createPrintMessageTask() - Can't create print message task -> print message task already exists");
    }
}
void Communication::deletePrintMessageTask() {
    // if (msPrintMessageTaskHandle != NULL) {
    //     vTaskDelete(msPrintMessageTaskHandle);
    //     msPrintMessageTaskHandle = NULL;
    // }
}
// ================================================================

// ========================== Check Sum ===========================

// char Communication::calculateCheckSum (char *message) {
//     uint16_t checkSum = 0;
//     for (uint8_t i = 1; i < strlen(message); i++) {
//         checkSum += (uint16_t)message[i];
//     }
//     Serial.print("\nmessage: ");
//     Serial.println(message);
//     Serial.print("calculateCheckSum: ");
//     Serial.println(checkSum);
//     return (char)checkSum;
// }
// bool Communication::checkMessage(char *message) {
//     Serial.println("checkMessage");
//     uint16_t checkSum = 0;
//     for (uint8_t i = 0; i < strlen(message); i++) {
//         checkSum += (uint16_t)message[i];
//     }
//     return (checkSum % 256 == 0);
// }
// ================================================================

