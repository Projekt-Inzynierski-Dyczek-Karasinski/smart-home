#include "hc12.h"

#include <Arduino.h>
#include <HardwareSerial.h>
#include <memory>

#include "config/communication_config.h"

#include "../utils/uint8_array_handlers.h"
#include "communication/communication.h"

namespace uah = Utils::ArrayHandlers;

namespace Comms {
    HC12* HC12::mspHC12 = nullptr;

// ============================ Public ============================

    HC12::HC12(Communication *communication, const std::shared_ptr<ul::Logger> &logger) {
        mpCommunication = communication;
        mspHC12 = this;
        mpLogger = logger;

        pinMode(SET_PIN, OUTPUT);
        digitalWrite(SET_PIN, HIGH);
        vTaskDelay(pdMS_TO_TICKS(DELAY_AFTER_SET_PIN_HIGH));

        mSendingDataMutex = xSemaphoreCreateMutex();

        createQueues();

        mBaudRate = (unsigned long)BAUD_RATE;
        mpSerial = new HardwareSerial(HARDWARE_SERIAL_UART_NR);
        mpSerial->begin(mBaudRate, SERIAL_8N1, RX_PIN, TX_PIN);

        createTransmitTask();
        createHC12MainTask();

        mpLogger->info("HC12 Class", "HC12 initialized.");
    }

    HC12::~HC12() {
        deleteHC12MainTask();
        deleteTransmitTask();
        deleteSetupHC12Task();

        deleteQueues();
        deleteSetupHC12Queues();

        vSemaphoreDelete(mSendingDataMutex);

        digitalWrite(SET_PIN, LOW);

        delete mpSerial;
    }

    void HC12::addMessageToTransmit(const uint8_t *message) const {
        xQueueSend(mTransmitQueue, message, portMAX_DELAY);
        vTaskResume(mTransmitTaskHandle);
    }

    void HC12::setupHC12(const uint8_t *commands) {
        if (!(commands[0] == 'H' && commands[1] == 'C')) {
            mpLogger->error("HC12 Method", "HC12 commands passed in setupHC12() must start with 'H', 'C'");
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
                        mpLogger->error("HC12 Method", "Passed too many commands in setupHC12().");
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
                    mpLogger->error("HC12 Method", "Passed too many commands in setupHC12().");
                } else {
                    uah::prepareBuffer(commandBuffer, &commands[commandStartIndex], (commandEndIndex - commandStartIndex), SETUP_COMMAND_SIZE);
                    xQueueSend(mSetupHC12CommandsQueue, commandBuffer, portMAX_DELAY);
                }
            }
            constexpr uint8_t notificationValue = CREATE_SETUP_HC12_TASK_NOTIF;
            xQueueSend(mMainNotificationsQueue, &notificationValue, portMAX_DELAY);
        }
    }

    void HC12::changeRFChannel(uint8_t channel) {
        if (channel < DEFAULT_CHANNEL || channel > MAX_CHANNEL) {
            mpLogger->errorv("HC12 Method", "RF channel on HC12 module must be set between 1 - 127, but got:", channel);
            channel = DEFAULT_CHANNEL;
        }

        uint8_t commandBuffer[8];
        char messageBuffer[8];
        sprintf(messageBuffer, "HC+C%03u", channel);
        uah::prepareBuffer(commandBuffer, (uint8_t*)messageBuffer, 7, SETUP_COMMAND_SIZE);
        setupHC12(commandBuffer);
    }
// ================================================================

// ============================ Queues ============================
    void HC12::createQueues() {
        if (mMainNotificationsQueue == nullptr) {
            mMainNotificationsQueue = xQueueCreate(NOTIFICATIONS_QUEUE_SIZE, sizeof(uint8_t));
        }
        if (mTransmitQueue == nullptr) {
            mTransmitQueue = xQueueCreate(PROTOCOL_MESSAGE_MAX_NUM, sizeof(uint8_t[PROTOCOL_SIZE]));
        }
    }

    void HC12::deleteQueues() {
        if (mTransmitQueue != nullptr) {
            vQueueDelete(mTransmitQueue);
            mTransmitQueue = nullptr;
        }
        if (mMainNotificationsQueue != nullptr) {
            vQueueDelete(mMainNotificationsQueue);
            mMainNotificationsQueue = nullptr;
        }
    }

    void HC12::createSetupHC12Queues() {
        if (mSetupHC12CommandsQueue == nullptr) {
            mSetupHC12CommandsQueue = xQueueCreate(SETUP_MAX_NUM_OF_COMMANDS, sizeof(uint8_t[SETUP_COMMAND_SIZE]));
        }
        if (mSetupHC12ReceiveQueue == nullptr) {
            mSetupHC12ReceiveQueue = xQueueCreate(SETUP_MAX_LEN_OF_RESPONSE, sizeof(uint8_t));
        }
    }

    void HC12::deleteSetupHC12Queues() {
        if (mSetupHC12CommandsQueue != nullptr) {
            vQueueDelete(mSetupHC12CommandsQueue);
            mSetupHC12CommandsQueue = nullptr;
        }
        if (mSetupHC12ReceiveQueue != nullptr) {
            vQueueDelete(mSetupHC12ReceiveQueue);
            mSetupHC12ReceiveQueue = nullptr;
        }
    }

// ================================================================

// ========================== HC12 Main ===========================

    void HC12::hc12OutputDecider(const uint8_t *hc12Output, const bool *isSetupHC12Working, bool *isWaitingForSendConfirmation) const {
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

        uint8_t status = DEFAULT_STATUS_NOTIF;
        bool isSetupHC12Working = false;
        bool isWaitingForSendConfirmation = false;

        // clear random hc12 output after powering on
        vTaskDelay(pdMS_TO_TICKS(DELAY_AFTER_SET_PIN_HIGH));
        while (hc12.mpSerial->available() > 0) {
            hc12.mpSerial->read();
        }

        for (;;) {
            // change status
            if (xQueueReceive(hc12.mMainNotificationsQueue, &status, 0) == pdFALSE) {
                // reset notifications status if there is no notifications
                status = DEFAULT_STATUS_NOTIF;
            }

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
                    hc12.mpLogger->debug("HC12 Main", "vTaskSuspend(hc12.mTransmitTaskHandle)");
                    vTaskSuspend(hc12.mTransmitTaskHandle);
                    break;

                case CREATE_SETUP_HC12_TASK_NOTIF:
                    hc12.mpLogger->debug("HC12 Main", "CREATE_SETUP_HC12_TASK_NOTIF");
                    isSetupHC12Working = true;
                    hc12.createSetupHC12Task();
                    break;

                case DELETE_SETUP_HC12_TASK_NOTIF:
                    hc12.mpLogger->debug("HC12 Main", "DELETE_SETUP_HC12_TASK_NOTIF");
                    isSetupHC12Working = false;
                    hc12.deleteSetupHC12Task();
                    break;

                default:
                    hc12.mpLogger->errorv("HC12 Main", "Got unknow status. Received Status: ", status);
                    break;
            }
        }
    }

    void HC12::createHC12MainTask() {
        if (mHC12MainTaskHandle == nullptr) {
            xTaskCreate(
                HC12MainTask,
                "HC12 Main Task",
                2048,
                nullptr,
                BACKGROUND_TASK_PRIORITY,
                &mHC12MainTaskHandle
            );
        } else {
            mpLogger->warning("HC12 FreeRTOS", "Can't create HC12 Main task, because task already exists");
        }
    }
    void HC12::deleteHC12MainTask() {
        if (mHC12MainTaskHandle != nullptr) {
            vTaskDelete(mHC12MainTaskHandle);
            mHC12MainTaskHandle = nullptr;
        }
    }
// ================================================================

// ========================== Send Task ===========================

    void HC12::transmitTask(void *parameters) {
        const auto &hc12 = *mspHC12;

        uint8_t transmitBuffer[PROTOCOL_SIZE];

        for (;;) {
            if (xQueueReceive(hc12.mTransmitQueue, transmitBuffer, pdMS_TO_TICKS(SUSPEND_TASK_TIME_SHORT)) == pdTRUE) {
                xSemaphoreTake(hc12.mSendingDataMutex, portMAX_DELAY);

                // TODO consider making this delay more "intelligent" (eg. by cooldown timer)
                // this delay is required for HC12 transmit/receive message properly
                vTaskDelay(pdMS_TO_TICKS(DELAY_BETWEEN_MESSAGES));

                // TODO remove?
                uint32_t hc12Respond;
                // clearing old notification (if exist)
                xTaskNotifyWait(0, ULONG_MAX, &hc12Respond, 0);
                constexpr uint8_t notificationValue = WAITING_FOR_SEND_CONFIRMATION_NOTIF;
                xQueueSendToFront(hc12.mMainNotificationsQueue, &notificationValue, portMAX_DELAY);

                // transmitting data
                hc12.mpSerial->write(transmitBuffer, PROTOCOL_SIZE);

                // TODO change?
                // wait for confirmation from HC12
                if (xTaskNotifyWait(0, ULONG_MAX, &hc12Respond, pdMS_TO_TICKS(RECEIVE_BYTE_TIMEOUT)) == pdTRUE) {
                    hc12.mpLogger->warning("HC12 Transmit", "HC12 module may have insufficient power.");
                } else {
                    constexpr uint8_t notificationValue1 = CANCEL_WAITING_FOR_SEND_CONFIRMATION_NOTIF;
                    xQueueSendToFront(hc12.mMainNotificationsQueue, &notificationValue1, portMAX_DELAY);
                }

                xSemaphoreGive(hc12.mSendingDataMutex);
            } else {
                constexpr uint8_t notificationValue2 = SUSPEND_TRANSMIT_TASK_NOTIF;
                xQueueSend(hc12.mMainNotificationsQueue, &notificationValue2, portMAX_DELAY);
            }
        }
    }

    void HC12::createTransmitTask() {
        if (mTransmitTaskHandle == nullptr) {
            xTaskCreate(
                transmitTask,
                "HC12 Transmit Task",
                2048,
                nullptr,
                HIGH_TASK_PRIORITY,
                &mTransmitTaskHandle
            );
        } else {
            mpLogger->warning("HC12 FreeRTOS", "Can't create transmit task, because task already exists");
        }
    }

    void HC12::deleteTransmitTask() {
        if (mTransmitTaskHandle != nullptr) {
            vTaskDelete(mTransmitTaskHandle);
            mTransmitTaskHandle = nullptr;
        }
    }
// ================================================================

// ========================== Setup HC12 ==========================

    void HC12::setupHC12Task(void *parameters) {
        const auto &hc12 = *mspHC12;

        // prepare commandBuffer
        uint8_t commandBuffer[SETUP_COMMAND_SIZE];
        for (uint8_t i = 0; i < SETUP_COMMAND_SIZE; i++) {
            commandBuffer[i] = 0;
        }

        for (;;) {
            if (xQueueReceive(hc12.mSetupHC12CommandsQueue, commandBuffer, 0) == pdTRUE) {
                hc12.mpLogger->infoa("HC12 Setup", "Received command: ", commandBuffer, SETUP_COMMAND_SIZE);

                // TODO add better protection against bad commands
                if (!(commandBuffer[0] == (uint8_t)'H' && commandBuffer[1] == (uint8_t)'C')) {
                    hc12.mpLogger->error("HC12 Setup", "Received array is not hc12 command.");
                } else {
                    const uint8_t lenOfCommand = uah::calcLenOfDataInArray(commandBuffer, SETUP_COMMAND_SIZE);
                    commandBuffer[0] = (uint8_t)'A';
                    commandBuffer[1] = (uint8_t)'T';

                    hc12.mpSerial->write(commandBuffer, lenOfCommand);

                    uint8_t hc12Response[SETUP_MAX_LEN_OF_RESPONSE];
                    uint8_t index = 0;
                    uah::clearBuffer(hc12Response, SETUP_MAX_LEN_OF_RESPONSE);

                    for (;;) {
                        if (xQueueReceive(hc12.mSetupHC12ReceiveQueue, &hc12Response[index], pdMS_TO_TICKS(RECEIVE_BYTE_TIMEOUT)) == pdTRUE) {
                            index++;
                        } else {
                            break;
                        }
                    }

                    if (index == 0) {
                        hc12.mpLogger->error("HC12 Setup", "HC12 module is not responding.");
                    } else {
                        hc12.mpLogger->infoa("HC12 Setup", "HC12 response: ", hc12Response, SETUP_MAX_LEN_OF_RESPONSE);
                    }
                }
            } else {
                constexpr uint8_t notificationValue = DELETE_SETUP_HC12_TASK_NOTIF;
                xQueueSend(hc12.mMainNotificationsQueue, &notificationValue, portMAX_DELAY);
                for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
            }
        }
    }

    void HC12::createSetupHC12Task() {
        xSemaphoreTake(mSendingDataMutex, portMAX_DELAY);
        digitalWrite(SET_PIN, LOW);
        vTaskDelay(pdMS_TO_TICKS(DELAY_AFTER_SET_PIN_LOW));

        if (mSetupHC12TaskHandle == nullptr) {
            xTaskCreate(
                setupHC12Task,
                "HC12 Setup Task",
                2048,
                nullptr,
                HIGH_TASK_PRIORITY,
                &mSetupHC12TaskHandle
            );
        } else {
            mpLogger->warning("HC12 FreeRTOS", "Can't create setup HC12 task, because task already exist.");
        }
    }
    void HC12::deleteSetupHC12Task() {
        if (mSetupHC12TaskHandle != nullptr) {
            vTaskDelete(mSetupHC12TaskHandle);
            mSetupHC12TaskHandle = nullptr;
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
}