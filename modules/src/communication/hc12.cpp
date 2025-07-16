#include "communication/hc12.h"

#include <Arduino.h>
#include <HardwareSerial.h>

#include "smart_home_config.h"
#include "config/communication_config.h"

// ============================ Public ============================

HC12& HC12::getInstance() {
    static HC12 instance;
    return instance;
}
 
void HC12::setupHC12(const uint8_t *COMMANDS) {
    if (!(COMMANDS[0] == 'H' && COMMANDS[1] == 'C')) {
        Serial.println("HC12 COMMAND ERROR! In setupHC12() -> passed argument does not include hc12 command.");
    } else {
        createSetupHC12Queues();

        // split multiple command in COMMANDS array
        uint8_t commandStartIndex = 0;
        uint8_t commandEndIndex = 0;
        uint8_t commandBuffor[SETUP_COMMAND_SIZE];
        uint8_t commandCounter = 0;
        while (COMMANDS[commandEndIndex] != 0) {
            if (COMMANDS[commandEndIndex] == (uint8_t)'|') {
                commandCounter++;
                if (commandCounter > SETUP_MAX_NUM_OF_COMMANDS) {
                    Serial.print("HC12 COMMANDS ERROR! In setupHC12() -> passed too many commands. Number of commands must be lower or equal than ");
                    Serial.println(SETUP_MAX_NUM_OF_COMMANDS);
                    break;
                }
                prepareCommandBuffor(commandBuffor, &COMMANDS[commandStartIndex], (commandEndIndex - commandStartIndex));

                xQueueSend(mSetupHC12CommandsQueue, commandBuffor, portMAX_DELAY);
                commandStartIndex = commandEndIndex + 1;
            }
            
            commandEndIndex++;
        }
        
        if (COMMANDS[commandEndIndex - 1] != (uint8_t)'|') {
            commandCounter++;
            if (commandCounter > SETUP_MAX_NUM_OF_COMMANDS) {
                Serial.print("HC12 COMMANDS ERROR! In setupHC12() -> passed too many commands. Number of commands must be lower or equal than ");
                Serial.println(SETUP_MAX_NUM_OF_COMMANDS);
            } else {
                prepareCommandBuffor(commandBuffor, &COMMANDS[commandStartIndex], (commandEndIndex - commandStartIndex));
                xQueueSend(mSetupHC12CommandsQueue, commandBuffor, portMAX_DELAY);
            }
        }

        xTaskNotify(mHC12MainTaskHandle, createSetupHC12TaskNotif, eSetValueWithOverwrite);
    }
}
// ================================================================

// ================== Constructor and Destructor ==================

HC12::HC12() {
    pinMode(SET_PIN, OUTPUT);
    digitalWrite(SET_PIN, HIGH);
    // 80ms - from HC12 documentation 
    vTaskDelay(pdMS_TO_TICKS(80));

    // createQueues();
    
    mBaudRate = (unsigned long)BAUD_RATE;
    mpSerial = new HardwareSerial(HARDWARE_SERIAL_UART_NR);
    mpSerial->begin(mBaudRate, SERIAL_8N1, RX_PIN, TX_PIN);
    // vTaskDelay(pdMS_TO_TICKS(1000));
    
    createHC12MainTask();
    Serial.println("HC12 initialized");
}


HC12::~HC12() {
    deleteHC12MainTask();
    deleteSetupHC12Task();

    // deleteQueues();
    deleteSetupHC12Queues();

    digitalWrite(SET_PIN, LOW);

    delete mpSerial;
}
// ================================================================

// ============================ Queues ============================

// void HC12::createQueues() {
//     // TODO assign propper length of queue 
//     if (mSetupHC12Queue == NULL) {
//         mSetupHC12Queue = xQueueCreate(5, sizeof(uint8_t[SETUP_COMMAND_SIZE]));
//     }
// }

// void HC12::deleteQueues() {
//     if (mSetupHC12Queue != NULL) {
//         vQueueDelete(mSetupHC12Queue);
//         mSetupHC12Queue = NULL;
//     }
// }

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

void HC12::HC12MainTask(void *parameters) {
    auto &hc12 = HC12::getInstance();

    uint32_t status = defaultStatusNotif;
    bool isSetupHC12Working = false;

    for (;;) {
        // change status
        xTaskNotifyWait(0, ULONG_MAX, &status, 0);
        
        uint8_t hc12Input;
        switch (status) {
            case defaultStatusNotif:
                if (hc12.mpSerial->available() > 0) {
                    hc12Input = hc12.mpSerial->read();

                    if (isSetupHC12Working) {
                        xQueueSend(hc12.mSetupHC12ReceiveQueue, &hc12Input, portMAX_DELAY);
                    }
                }

                // delay for watchdog 
                vTaskDelay(pdMS_TO_TICKS(1));
                break;

            case createSetupHC12TaskNotif:
                // TODO remove print
                Serial.println("createSetupHC12TaskNotif");
                isSetupHC12Working = true;
                hc12.createSetupHC12Task();
                break;

            case deleteSetupHC12TaskNotif:
                // TODO remove print
                Serial.println("deleteSetupHC12TaskNotif");
                isSetupHC12Working = false;
                hc12.deleteSetupHC12Task();
                hc12.deleteSetupHC12Queues();
                break;

            default:
                Serial.print("STATUS ERROR! In HC12MainTask() -> got unknow status. Received Status: ");
                Serial.println(status);
                break;
        }

        // reset notifications status 
        status = defaultStatusNotif;        
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

// ========================== setup HC12 ==========================

void HC12::setupHC12Task(void *parameters) {
    auto &hc12 = HC12::getInstance();

    // TODO remove ?
    /*
    char 'x' (and next chars if needed) must be changed before sending command to hc12
    0  - "AT"
    1  - "AT+RB"
    2  - "AT+RC"
    3  - "AT+RF"
    4  - "AT+RP"
    5  - "AT+RX"
    6  - "AT+SLEEP"
    7  - "AT+DEFAULT"
    8  - "AT+Bx"
    9  - "AT+Cx"
    10 - "AT+FUx"
    11 - "AT+Px"
    */
    // const uint8_t COMMANDS[][10] = {
    //     {'A', 'T', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0'},
    //     {'A', 'T', '+', 'R', 'B', '\0', '\0', '\0', '\0', '\0'},
    //     {'A', 'T', '+', 'R', 'C', '\0', '\0', '\0', '\0', '\0'},
    //     {'A', 'T', '+', 'R', 'F', '\0', '\0', '\0', '\0', '\0'},
    //     {'A', 'T', '+', 'R', 'P', '\0', '\0', '\0', '\0', '\0'},
    //     {'A', 'T', '+', 'R', 'X', '\0', '\0', '\0', '\0', '\0'},
    //     {'A', 'T', '+', 'S', 'L', 'E', 'E', 'P', '\0', '\0'},
    //     {'A', 'T', '+', 'D', 'E', 'F', 'A', 'U', 'L', 'T'},
    //     {'A', 'T', '+', 'B', 'x', '\0', '\0', '\0', '\0', '\0'},
    //     {'A', 'T', '+', 'C', 'x', '\0', '\0', '\0', '\0', '\0'},
    //     {'A', 'T', '+', 'F', 'U', 'x', '\0', '\0', '\0', '\0'},
    //     {'A', 'T', '+', 'P', 'x', '\0', '\0', '\0', '\0', '\0'},
    // };

    // prepare commandBuffor
    uint8_t commandBuffor[SETUP_COMMAND_SIZE];
    for (uint8_t i = 0; i < SETUP_COMMAND_SIZE; i++) {
        commandBuffor[i] = 0;
    }
    // prepare receiveBuffor
    uint8_t receiveByteBuffor = 0;
    uint8_t receiveBuffor[MESSAGE_SIZE];
    for (uint8_t i = 0; i < MESSAGE_SIZE; i++) {
        receiveBuffor[i] = 0;
    }

    for (;;) {
        if (xQueueReceive(hc12.mSetupHC12CommandsQueue, commandBuffor, 0) == pdTRUE) {
            Serial.print("setupHC12Task received command: ");
            hc12.printUint8Array(commandBuffor, SETUP_COMMAND_SIZE);

            if (!(commandBuffor[0] == (uint8_t)'H' && commandBuffor[1] == (uint8_t)'C')) {
                Serial.println("HC12 COMMAND ERROR! In setupHC12Task() -> received array is not hc12 command.");
            } else {
                uint8_t lenOfCommand = hc12.calcLenOfUint8Array(commandBuffor, SETUP_COMMAND_SIZE);
                commandBuffor[0] = (uint8_t)'A';
                commandBuffor[1] = (uint8_t)'T';
                
                hc12.mpSerial->write(commandBuffor, lenOfCommand);

                bool hasHC12Responded = false;
                uint8_t hc12Response;
                for (;;) {
                    if (xQueueReceive(hc12.mSetupHC12ReceiveQueue, &hc12Response, pdMS_TO_TICKS(RECEIVE_BYTE_TIMEOUT)) == pdTRUE) {
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
            xTaskNotify(hc12.mHC12MainTaskHandle, deleteSetupHC12TaskNotif, eSetValueWithOverwrite);
            for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}


void HC12::createSetupHC12Task() {
    digitalWrite(SET_PIN, LOW);
    // 40ms - from HC12 documentation 
    vTaskDelay(pdMS_TO_TICKS(40));
    // TODO !BEFORE PULL REQUEST! check is it working properly!
    if (mBaudRate != 9600) {
        mpSerial->updateBaudRate(9600);
    }

    if (mSetupHC12TaskHandle == NULL) {
        xTaskCreate(
            setupHC12Task,
            "Setup HC12 Task",
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
    // TODO !BEFORE PULL REQUEST! check is it working properly!
    if (mBaudRate != 9600) {
        mpSerial->updateBaudRate(mBaudRate);
    }

    digitalWrite(SET_PIN, HIGH);
    // 80ms - from HC12 documentation 
    vTaskDelay(pdMS_TO_TICKS(80));
}
// ================================================================

// ============================ other =============================
void HC12::prepareCommandBuffor(uint8_t *buffor, const uint8_t *value, uint8_t len) {
    if (len > SETUP_COMMAND_SIZE) {
        len = SETUP_COMMAND_SIZE;
        Serial.print("VALUE ERROR! In prepareMessageBuffor() -> len must not be larger than ");
        Serial.println(SETUP_COMMAND_SIZE);
    }

    for (uint8_t i = 0; i < len; i++) {
        buffor[i] = value[i];
    }
    for (uint8_t i = len; i < SETUP_COMMAND_SIZE; i++) {
        buffor[i] = 0;
    }
}
void HC12::printUint8Array(const uint8_t *array, const uint8_t len) {
    for (uint8_t i = 0; i < len; i++) {
        Serial.print((char)array[i]);
    }
    Serial.println();
}

uint8_t HC12::calcLenOfUint8Array(const uint8_t *array, const uint8_t maxLen) {
    for (uint8_t i = 0; i < maxLen; i++) {
        if (array[i] == 0) return i; 
    }
    return maxLen;
}

// bool Communication::areArraysEqual(const uint8_t *array1, const uint8_t *array2, uint8_t len) {
//     for (uint8_t i = 0; i < len; i++){
//         if (array1[i] != array2[i]) {
//             return false;
//         }
//     }
//     return true;
// }

// ================================================================