#include "communication/hc12.h"

#include <Arduino.h>
#include <HardwareSerial.h>

#include "smart_home_config.h"
#include "config/communication_config.h"

#include "communication/uint8_array_handlers.h"
#include "communication/communication.h"

namespace uah = uint8ArrayHandlers;

HC12* HC12::mspHC12 = nullptr;

// ============================ Public ============================

HC12::HC12(Communication *communication) {
    mpCommunication = communication;
    mspHC12 = this;

    pinMode(SET_PIN, OUTPUT);
    digitalWrite(SET_PIN, HIGH);
    vTaskDelay(pdMS_TO_TICKS(DELAY_AFTER_SET_PIN_HIGH));

    mSendingDataMutex = xSemaphoreCreateMutex();

    createQueue();
    
    mBaudRate = (unsigned long)BAUD_RATE;
    mpSerial = new HardwareSerial(HARDWARE_SERIAL_UART_NR);
    mpSerial->begin(mBaudRate, SERIAL_8N1, RX_PIN, TX_PIN);
    
    createTransmitTask();
    createHC12MainTask();
    Serial.println("HC12 initialized");
}


HC12::~HC12() {
    deleteHC12MainTask();
    deleteTransmitTask();
    deleteSetupHC12Task();

    deleteQueue();
    deleteSetupHC12Queues();

    digitalWrite(SET_PIN, LOW);

    delete mpSerial;
}

void HC12::addMessageToTransmit(const uint8_t *message) {
    xQueueSend(mTransmitQueue, message, portMAX_DELAY);
    vTaskResume(mTransmitTaskHandle);
}

void HC12::setupHC12(const uint8_t *commands) {
    if (!(commands[0] == 'H' && commands[1] == 'C')) {
        Serial.println("HC12 COMMAND ERROR! In setupHC12() -> passed argument does not include hc12 command.");
    } else {
        createSetupHC12Queues();

        // split multiple command in COMMANDS array
        uint8_t commandStartIndex = 0;
        uint8_t commandEndIndex = 0;
        uint8_t commandBuffer[SETUP_COMMAND_SIZE];
        uint8_t commandCounter = 0;
        while (commands[commandEndIndex] != 0) {
            if (commands[commandEndIndex] == (uint8_t)'|') {
                commandCounter++;
                if (commandCounter > SETUP_MAX_NUM_OF_COMMANDS) {
                    Serial.print("HC12 COMMANDS ERROR! In setupHC12() -> passed too many commands. Number of commands must be lower or equal than ");
                    Serial.println(SETUP_MAX_NUM_OF_COMMANDS);
                    break;
                }
                uah::prepareBuffer(commandBuffer, &commands[commandStartIndex], (commandEndIndex - commandStartIndex), SETUP_COMMAND_SIZE);

                xQueueSend(mSetupHC12CommandsQueue, commandBuffer, portMAX_DELAY);
                commandStartIndex = commandEndIndex + 1;
            }
            
            commandEndIndex++;
        }
        
        if (commands[commandEndIndex - 1] != (uint8_t)'|') {
            commandCounter++;
            if (commandCounter > SETUP_MAX_NUM_OF_COMMANDS) {
                Serial.print("HC12 COMMANDS ERROR! In setupHC12() -> passed too many commands. Number of commands must be lower or equal than ");
                Serial.println(SETUP_MAX_NUM_OF_COMMANDS);
            } else {
                uah::prepareBuffer(commandBuffer, &commands[commandStartIndex], (commandEndIndex - commandStartIndex), SETUP_COMMAND_SIZE);
                xQueueSend(mSetupHC12CommandsQueue, commandBuffer, portMAX_DELAY);
            }
        }

        xTaskNotify(mHC12MainTaskHandle, CREATE_SETUP_HC12_TASK_NOTIF, eSetValueWithOverwrite);
    }
}
// ================================================================

// ============================ Queues ============================

void HC12::createQueue() { 
    if (mTransmitQueue == NULL) {
        mTransmitQueue = xQueueCreate(PROTOCOL_MESSAGE_MAX_NUM, sizeof(uint8_t[PROTOCOL_SIZE]));
    }
}

void HC12::deleteQueue() {
    if (mTransmitQueue != NULL) {
        vQueueDelete(mTransmitQueue);
        mTransmitQueue = NULL;
    }
}

void HC12::createSetupHC12Queues() {
    if (mSetupHC12CommandsQueue == NULL) {
        mSetupHC12CommandsQueue = xQueueCreate(SETUP_MAX_NUM_OF_COMMANDS, sizeof(uint8_t[SETUP_COMMAND_SIZE]));
    }

    if (mSetupHC12ReceiveQueue == NULL) {
        mSetupHC12ReceiveQueue = xQueueCreate(SETUP_MAX_LEN_OF_RESPONSE, sizeof(uint8_t));
    }
}

void HC12::deleteSetupHC12Queues() {
    if (mSetupHC12CommandsQueue != NULL) {
        vQueueDelete(mSetupHC12CommandsQueue);
        mSetupHC12CommandsQueue = NULL;
    }

    if (mSetupHC12ReceiveQueue != NULL) {
        vQueueDelete(mSetupHC12ReceiveQueue);
        mSetupHC12ReceiveQueue = NULL;
    }
}

// ================================================================

// ========================== HC12 Main ===========================

void HC12::hc12OutputDecider(const uint8_t *hc12Output, const bool *isSetupHC12Working, bool *isWaitingForSendConfirmation) {
    if (*isWaitingForSendConfirmation) {
        *isWaitingForSendConfirmation = false;
        xTaskNotify(mTransmitTaskHandle, (uint32_t)*hc12Output, eSetValueWithOverwrite);
    } else if (*isSetupHC12Working) {
        xQueueSend(mSetupHC12ReceiveQueue, hc12Output, portMAX_DELAY);
    } else {
        mpCommunication->addByteToDecode(*hc12Output);
    } 
}

void HC12::HC12MainTask(void *parameters) {
    auto &hc12 = *mspHC12;

    uint32_t status = DEFAULT_STATUS_NOTIF;
    bool isSetupHC12Working = false;
    bool isWaitingForSendConfirmation = false;

    // clear random hc12 output after powering on
    vTaskDelay(pdMS_TO_TICKS(DELAY_AFTER_SET_PIN_HIGH));
    while (hc12.mpSerial->available() > 0) {
        hc12.mpSerial->read();
    }

    for (;;) {
        // TODO change this to queue (race conditions)
        // change status
        xTaskNotifyWait(0, ULONG_MAX, &status, 0);
        
        uint8_t hc12Output;
        switch (status) {
            case DEFAULT_STATUS_NOTIF:
                if (hc12.mpSerial->available() > 0) {
                    hc12Output = hc12.mpSerial->read();
                    hc12.hc12OutputDecider(&hc12Output, &isSetupHC12Working, &isWaitingForSendConfirmation);
                } else {
                    // delay for watchdog 
                    vTaskDelay(pdMS_TO_TICKS(1));
                }

                // extra protection if somehow queue is not empty and task is suspended 
                if (uxQueueMessagesWaiting(hc12.mTransmitQueue) != 0) {
                    vTaskResume(hc12.mTransmitTaskHandle);
                }
                break;

            case WAITING_FOR_SEND_CONFIRMATION_NOTIF:
                isWaitingForSendConfirmation = true;
                break;

            case CANCEL_WAITING_FOR_SEND_CONFIRMATION_NOTIF:
                isWaitingForSendConfirmation = false;
                break;

            case SUSPEND_TRANSMIT_TASK_NOTIF:
                // TODO remove print
                // Serial.println("vTaskSuspend(hc12.mTransmitTaskHandle);");
                vTaskSuspend(hc12.mTransmitTaskHandle);
                break;

            case CREATE_SETUP_HC12_TASK_NOTIF:
                // TODO remove print
                // Serial.println("CREATE_SETUP_HC12_TASK_NOTIF");
                isSetupHC12Working = true;
                hc12.createSetupHC12Task();
                break;

            case DELETE_SETUP_HC12_TASK_NOTIF:
                // TODO remove print
                // Serial.println("DELETE_SETUP_HC12_TASK_NOTIF");
                isSetupHC12Working = false;
                hc12.deleteSetupHC12Task();
                break;

            default:
                Serial.print("STATUS ERROR! In HC12MainTask() -> got unknow status. Received Status: ");
                Serial.println(status);
                break;
        }

        // reset notifications status 
        status = DEFAULT_STATUS_NOTIF;        
    }
}

void HC12::createHC12MainTask() {
    if (mHC12MainTaskHandle == NULL) {
        xTaskCreate(
            HC12MainTask,
            "HC12 Main Task",
            2048,
            NULL,
            BACKGROUND_TASK_PRIORITY,
            &mHC12MainTaskHandle
        );
    } else {
        Serial.println("TASK CREATION ERROR! In createHC12MainTask() -> Can't create HC12 Main task, because task already exists");
    }
}
void HC12::deleteHC12MainTask() {
    if (mHC12MainTaskHandle != NULL) {
        vTaskDelete(mHC12MainTaskHandle);
        mHC12MainTaskHandle = NULL;
    }
}
// ================================================================

// ========================== Send Task ===========================

void HC12::transmitTask(void *parameters) {
    auto &hc12 = *mspHC12;

    uint8_t transmitBuffer[PROTOCOL_SIZE];
    
    for (;;) {
        if (xQueueReceive(hc12.mTransmitQueue, transmitBuffer, pdMS_TO_TICKS(SUSPEND_TASK_TIME_SHORT)) == pdTRUE) {
            xSemaphoreTake(hc12.mSendingDataMutex, portMAX_DELAY);

            // TODO consider making this delay more "inteligent" (eg. by cooldown timer)
            // this delay is required for HC12 transmit/receive message properly
            vTaskDelay(pdMS_TO_TICKS(DELAY_BETWEEN_MESSAGES));

            // TODO remove?
            uint32_t hc12Respond;
            // clearing old notification (if exist)
            xTaskNotifyWait(0, ULONG_MAX, &hc12Respond, 0);
            // // transmiting data
            xTaskNotify(hc12.mHC12MainTaskHandle, WAITING_FOR_SEND_CONFIRMATION_NOTIF, eSetValueWithOverwrite);
            
            // transmiting data
            hc12.mpSerial->write(transmitBuffer, PROTOCOL_SIZE);

            // TODO remove?
            // wait for confirmation from HC12
            if (xTaskNotifyWait(0, ULONG_MAX, &hc12Respond, pdMS_TO_TICKS(RECEIVE_BYTE_TIMEOUT)) == pdTRUE) {
                if (hc12Respond != 255) {
                    Serial.print("TRANSMITING ERROR! In transmitTask() -> hc12 module did not confirm properly. HC12 module should send 255 signal but got: ");
                    Serial.println(hc12Respond);
                }
            } else {
                xTaskNotify(hc12.mHC12MainTaskHandle, CANCEL_WAITING_FOR_SEND_CONFIRMATION_NOTIF, eSetValueWithOverwrite);
                // TODO change
                // Serial.println("TRANSMITING ERROR! In transmitTask() -> hc12 module is not responding.");
                // Serial.println("HC12 NT");
            }

            xSemaphoreGive(hc12.mSendingDataMutex);
        } else {
            xTaskNotify(hc12.mHC12MainTaskHandle, SUSPEND_TRANSMIT_TASK_NOTIF, eSetValueWithOverwrite);
        }
    }
}

void HC12::createTransmitTask() {
    if (mTransmitTaskHandle == NULL) {
        xTaskCreate(
            transmitTask,
            "HC12 Transmit Task",
            2048,
            NULL,
            HIGH_TASK_PRIORITY,
            &mTransmitTaskHandle
        );
    } else {
        Serial.println("TASK CREATION ERROR! In createSetupHC12Task() -> Can't create setup HC12 task, because task already exists");
    }
}

void HC12::deleteTransmitTask() {
    if (mTransmitTaskHandle != NULL) {
        vTaskDelete(mTransmitTaskHandle);
        mTransmitTaskHandle = NULL;
    }
}
// ================================================================

// ========================== Setup HC12 ==========================

void HC12::setupHC12Task(void *parameters) {
    auto hc12 = mspHC12;

    // prepare commandBuffer
    uint8_t commandBuffer[SETUP_COMMAND_SIZE];
    for (uint8_t i = 0; i < SETUP_COMMAND_SIZE; i++) {
        commandBuffer[i] = 0;
    }
    // prepare receiveBuffer
    uint8_t receiveByteBuffer = 0;
    uint8_t receiveBuffer[MESSAGE_SIZE];
    for (uint8_t i = 0; i < MESSAGE_SIZE; i++) {
        receiveBuffer[i] = 0;
    }

    for (;;) {
        if (xQueueReceive(hc12->mSetupHC12CommandsQueue, commandBuffer, 0) == pdTRUE) {
            Serial.print("setupHC12Task received command: ");
            uah::printArrayAsChar(commandBuffer, SETUP_COMMAND_SIZE);

            // TODO add better protection against bad commands
            if (!(commandBuffer[0] == (uint8_t)'H' && commandBuffer[1] == (uint8_t)'C')) {
                Serial.println("HC12 COMMAND ERROR! In setupHC12Task() -> received array is not hc12 command.");
            } else {
                uint8_t lenOfCommand = uah::calcLenOfDataInArray(commandBuffer, SETUP_COMMAND_SIZE);
                commandBuffer[0] = (uint8_t)'A';
                commandBuffer[1] = (uint8_t)'T';
                
                hc12->mpSerial->write(commandBuffer, lenOfCommand);

                bool hasHC12Responded = false;
                uint8_t hc12Response;
                for (;;) {
                    if (xQueueReceive(hc12->mSetupHC12ReceiveQueue, &hc12Response, pdMS_TO_TICKS(RECEIVE_BYTE_TIMEOUT)) == pdTRUE) {
                        Serial.print((char)hc12Response);
                        hasHC12Responded = true;
                    } else {
                        break;
                    }
                }

                if (!hasHC12Responded) {
                    Serial.println("SETUP HC12 ERROR! In setupHC12Task() -> hc12 module is not responding.");
                }
            }
        } else {
            xTaskNotify(hc12->mHC12MainTaskHandle, DELETE_SETUP_HC12_TASK_NOTIF, eSetValueWithOverwrite);
            for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}


void HC12::createSetupHC12Task() {
    xSemaphoreTake(mSendingDataMutex, portMAX_DELAY);
    digitalWrite(SET_PIN, LOW);
    vTaskDelay(pdMS_TO_TICKS(DELAY_AFTER_SET_PIN_LOW));

    if (mSetupHC12TaskHandle == NULL) {
        xTaskCreate(
            setupHC12Task,
            "HC12 Setup Task",
            2048,
            NULL,
            HIGH_TASK_PRIORITY,
            &mSetupHC12TaskHandle
        );
    } else {
        Serial.println("TASK CREATION ERROR! In createSetupHC12Task() -> Can't create setup HC12 task, because task already exists");
    }
}
void HC12::deleteSetupHC12Task() {
    if (mSetupHC12TaskHandle != NULL) {
        vTaskDelete(mSetupHC12TaskHandle);
        mSetupHC12TaskHandle = NULL;
    }

    deleteSetupHC12Queues();

    digitalWrite(SET_PIN, HIGH);
    vTaskDelay(pdMS_TO_TICKS(DELAY_AFTER_SET_PIN_HIGH));
    // clear random hc12 output after changing state of SET_PIN
    while (mpSerial->available() > 0) {
        mpSerial->read();
    }
    xSemaphoreGive(mSendingDataMutex);
}
// ================================================================

// ============================ Other =============================

// ================================================================