// TODO !BEFORE PULL REQUEST! check for "eSetValueWithoutOverwrite"!
// TODO !BEFORE PULL REQUEST! change setting buffors "by hand" to using method prepareMessageBuffor()
#include "communication/communication.h"

#include <Arduino.h>
#include <HardwareSerial.h>

#include "smart_home_config.h"
#include "config/communication_config.h"

#include "universal_module_system/debug_led.h"
#ifdef HC12_MODULE
    #include "communication/hc12.h"
#endif

DebugLED* Communication::mspDebugLED = nullptr;

// #ifdef HC12_MODULE
//     HC12* Communication::mRfModule = nullptr;
// #else
//     #error "Not implemented"
// #endif


// ============================ Public ============================

Communication& Communication::getInstance(DebugLED *debugLED) {
    static Communication instance(debugLED);
    return instance;
}

// TODO implement
void Communication::startAddresingAlgorithm() {
    Serial.println("startAddresingAlgorithm() not implemented");
}
// ================================================================
void Communication::test() {
    Serial.println("Print from Communication class");
}
// ================== Constructor and Destructor ==================

Communication::Communication(DebugLED *debugLED) : 
    #ifdef HC12_MODULE
        mRfModule(new HC12(this)) 
    #else
        #error "Not implemented" 
    #endif
{
    mspDebugLED = debugLED;
    

    createSendCustomMessageTask();
    Serial.println("Communication initialized");
}

Communication::~Communication() {
    deleteSendCustomMessageTask();
}

// ================================================================

// ===================== Send Custom Message ======================

void Communication::sendCustomMessageTask(void *parameters) {
    auto &c = Communication::getInstance(mspDebugLED);

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

                // check if is HC_12 command
                #ifdef HC12_MODULE
                if (buffor[0] == 'A' && buffor[1] == 'T') {
                    buffor[0] = 'H';
                    buffor[1] = 'C';
                    c.mRfModule->setupHC12(buffor);
                } else {
                    // TODO add message to SendMessagesQueue
                    // xQueueSend(msSendMessagesQueue, &buffor, portMAX_DELAY);
                    Serial.println("// TODO add message to SendMessagesQueue");
                }
                #else
                // add message to SendMessagesQueue
                xQueueSend(msSendMessagesQueue, &buffor, portMAX_DELAY);
                #endif

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
