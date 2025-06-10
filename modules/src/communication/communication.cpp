#include "communication/communication.h"
#include "smart_home_config.h"
#include <HardwareSerial.h>


HardwareSerial* Communication::mspSerial = nullptr;
char Communication::msMACAddress[12];

TaskHandle_t Communication::msReceiveMessageTaskHandle = NULL;
TaskHandle_t Communication::msPrintMessageTaskHandle = NULL;
// TODO remove "msReceiveMessageBuffer"
// MessageBufferHandle_t Communication::msReceiveMessageBuffer = NULL;
QueueHandle_t Communication::msReceiveMessageQueue = NULL;

// TODO remove "testHC12" methods
TaskHandle_t Communication::msTestHC12TaskHandle = NULL;
// TODO remove "sendCustomMessage" methods
TaskHandle_t Communication::msSendCustomMessageTaskHandle = NULL;

Communication::Communication() {
    // Get MAC address
    #ifdef ESP32_BOARD
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        sprintf(msMACAddress, "%02X%02X%02X%02X%02X%02X",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
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
    vTaskDelay(pdMS_TO_TICKS(100));
    
    mspSerial = new HardwareSerial(HARDWARE_SERIAL_UART_NR);
    mspSerial->begin((unsigned long)BAUD_RATE, SERIAL_8N1, RX_PIN, TX_PIN);

    

    // TODO remove "testHC12" methods
    // createTestHC12Task();

    // TODO remove "msReceiveMessageBuffer"
    // createReceiveMessageBuffer();

    createReceiveMessageQueue();
    createPrintMessageTask();
    createReceiveMessageTask();
    createSendCustomMessageTask();
}

Communication::~Communication() {
    delete mspSerial;
    mspSerial = nullptr;

    // TODO remove "testHC12" methods
    deleteTestHC12Task();

    // TODO remove "sendCustomMessage" methods
    deleteSendCustomMessageTask();

    deleteReceiveMessageTask();
    // TODO remove "msReceiveMessageBuffer"
    // deleteReceiveMessageBuffer();

    digitalWrite(SET_PIN, LOW);
}

// ==================== Receive Message Queue =====================

void Communication::createReceiveMessageQueue() {
    if (msReceiveMessageQueue == NULL) {
        msReceiveMessageQueue = xQueueCreate(10, sizeof(char[64]));
    }
}
void Communication::deleteReceiveMessageQueue() {
    if (msReceiveMessageQueue != NULL) {
        vQueueDelete(msReceiveMessageQueue);
        msReceiveMessageQueue = NULL;
    }
}
// ================================================================

// TODO remove "msReceiveMessageBuffer"

// ==================== Receive Message Buffer ====================

// void Communication::createReceiveMessageBuffer() {
//     if (msReceiveMessageBuffer == NULL) {
//         msReceiveMessageBuffer = xMessageBufferCreate(64 +  sizeof( size_t ));
//     }
// }
// void Communication::deleteReceiveMessageBuffer() {
//     if (msReceiveMessageBuffer != NULL) {
//         vMessageBufferDelete(msReceiveMessageBuffer);
//         msReceiveMessageBuffer = NULL;
//     }
// }
// ================================================================

// ======================= Receive Message ========================

void Communication::receiveMessageTask() {
    char buffer[64];
    for (int i = 0; i < sizeof(buffer); i++) {
        buffer[i] = '\0';
    }
    uint8_t i = 0;
    int intBuffer[64];

    for(;;) {
        while(mspSerial->available()) {
            // fix for � char 
            buffer[i] = mspSerial->read();
            if ((int)buffer[i] > 250) {
                buffer[i] = '\0';
            } else {
                intBuffer[i] = (int)buffer[i];
                if (buffer[i] == '\n') {                
                    break;
                }
                i++;
            }
        }

        if (i > 0) {
            Serial.println("Received: " + String(buffer));
            Serial.print("Received (int): ");
            for (int j = 0; j < i; j++) {
                Serial.print(intBuffer[j]);
            }
            Serial.println("-----------");
            // TODO remove "msReceiveMessageBuffer"
            // xMessageBufferSend(msReceiveMessageBuffer, &buffer, sizeof(buffer), portMAX_DELAY);
            xQueueSend(msReceiveMessageQueue, &buffer, portMAX_DELAY);

            for (int j = 0; j <= i; j++) {
                buffer[j] = '\0';
            }
            i = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
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
    // TODO remove "msReceiveMessageBuffer"
    // if (msReceiveMessageBuffer != NULL) {
    //     vMessageBufferDelete(msReceiveMessageBuffer);
    //     msReceiveMessageBuffer = NULL;
    // }
}
// ================================================================

// ============================= test =============================
void Communication::printMessageTask() {
    char receivedData[64];
    for(;;) {
        if (xQueueReceive(msReceiveMessageQueue, &receivedData, portMAX_DELAY) == pdTRUE) {
            Serial.println("xQueueReceive: " + String(receivedData));
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
        Serial.println("void createPrintMessageTask() - Can't create print message task -> print message task already exists");
    }
}
void Communication::deletePrintMessageTask() {
    if (msPrintMessageTaskHandle != NULL) {
        vTaskDelete(msPrintMessageTaskHandle);
        msPrintMessageTaskHandle = NULL;
    }
}
// ================================================================

// TODO remove "testHC12" methods

// =========================== testHC12 ===========================

void Communication::testHC12Task() {
    digitalWrite(SET_PIN, LOW);
    for(;;) {
        while(mspSerial->available()) {
            Serial.write(mspSerial->read());
        }
        while(Serial.available()) {
            mspSerial->write(Serial.read());
        }
    }
}
void Communication::createTestHC12TaskHandle(void *parameters) {
    Communication* instance = static_cast<Communication*>(parameters);
    instance->testHC12Task();
}
void Communication::createTestHC12Task() {
    if (msTestHC12TaskHandle == NULL) {
        xTaskCreate(
            createTestHC12TaskHandle,
            "Test HC12",
            2048,
            NULL,
            0,
            &msTestHC12TaskHandle
        );
    } else {
        Serial.println("void createTestHC12Task() - Can't create Test HC12 task -> Test HC12 task already exists");
    }
}
void Communication::deleteTestHC12Task() {
    if (msTestHC12TaskHandle != NULL) {
        vTaskDelete(msTestHC12TaskHandle);
        msTestHC12TaskHandle = NULL;
    }
    digitalWrite(SET_PIN, HIGH);
}
// ================================================================

// TODO remove "sendCustomMessage" methods

// ===================== Send Custom Message ======================

void Communication::sendCustomMessageTask() {
    char buffer[64];
    // for (int i = 0; i < 12; i++) {
    //     buffer[i] = msMACAddress[i];
    // }
    for (int i = 0; i < sizeof(buffer); i++) {
        buffer[i] = '\0';
    }
    uint8_t i = 0;

    for (;;) {
        if (Serial.available()) {
            buffer[i] = Serial.read();
            if (buffer[i] == '\n') {
                Serial.println("Sending: " + String(buffer));
                mspSerial->write(buffer);
                for (int j = 0; j <= i; j++) {
                    buffer[j] = '\0';
                }
                i = 0;
            }else {

                i++;
            }
        }
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