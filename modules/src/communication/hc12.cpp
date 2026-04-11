#include "hc12.h"

#include "communication/communication.h"
#include "universal_module_system/data_manager.h"
#include "universal_module_system/power_manager/power_manager.h"

namespace uah = Utils::ArrayHandlers;
namespace ums = UniversalModuleSystem;

// TODO !pr add fu mode in base_config.json
namespace Comms {
    // ============================ Public ============================
    HC12::HC12(Communication *communication, const std::shared_ptr<ul::Logger> &logger)
        : mpCommunication(communication),
          mpLogger(logger),
          m_HC12_DATA(
              ums::DataManager::getInstance().loadJson(ums::DataManager::getInstance().s_BASE_CONFIG_PATH)[s_HC12_DATA]
          ) {
        pinMode(m_HC12_DATA.setPin, OUTPUT);

        mSendingDataMutex = xSemaphoreCreateMutex();
        mSetupWorkingSemaphore = xSemaphoreCreateBinary();
        mFirstSetupSemaphore = xSemaphoreCreateBinary();
        xSemaphoreGive(mFirstSetupSemaphore);

        createQueues();

        mpSerial = new HardwareSerial(HARDWARE_SERIAL_UART_NR);
        mpSerial->begin(
            sm_BAUD_RATES[ms_DEFAULT_BAUD_RATE_INDEX],
            SERIAL_8N1,
            m_HC12_DATA.rxPin,
            m_HC12_DATA.txPin
        );

        createSetupHC12Queues();
        createHC12MainTask();

        mpLogger->verbose("HC12 Class", "HC12 initialized.");
    }

    HC12::~HC12() {
        deleteHC12MainTask();
        deleteTransmitTask();
        deleteSetupHC12Task();

        deleteQueues();
        deleteSetupHC12Queues();

        vSemaphoreDelete(mSendingDataMutex);
        vSemaphoreDelete(mFirstSetupSemaphore);
        vSemaphoreDelete(mSetupWorkingSemaphore);

        digitalWrite(m_HC12_DATA.setPin, LOW);

        delete mpSerial;
    }

    void HC12::addMessageToTransmit(const uint8_t *message) const {
        xQueueSend(mTransmitQueue, message, portMAX_DELAY);
        if (mTransmitTaskHandle != nullptr)
            vTaskResume(mTransmitTaskHandle);
    }

    void HC12::setupHC12(const uint8_t *commands) const {
        if (!(commands[0] == 'H' && commands[1] == 'C')) {
            mpLogger->error("HC12 Method", "HC12 commands passed in setupHC12() must start with 'H', 'C'");
        } else {
            // split multiple command in COMMANDS array
            uint8_t commandStartIndex = 0;
            uint8_t commandEndIndex = 0;
            uint8_t commandBuffer[SETUP_COMMAND_SIZE];
            uint8_t commandCounter = 0;
            while (commands[commandEndIndex] != 0) {
                if (commands[commandEndIndex] == (uint8_t) '|') {
                    commandCounter++;
                    if (commandCounter > SETUP_MAX_NUM_OF_COMMANDS) {
                        mpLogger->error("HC12 Method", "Passed too many commands in setupHC12().");
                        break;
                    }
                    uah::prepareBuffer(
                        commandBuffer,
                        &commands[commandStartIndex],
                        (commandEndIndex - commandStartIndex),
                        SETUP_COMMAND_SIZE
                    );

                    xQueueSend(mSetupHC12CommandsQueue, commandBuffer, portMAX_DELAY);
                    commandStartIndex = commandEndIndex + 1;
                }

                commandEndIndex++;
            }

            if (commands[commandEndIndex - 1] != (uint8_t) '|') {
                commandCounter++;
                if (commandCounter > SETUP_MAX_NUM_OF_COMMANDS) {
                    mpLogger->error("HC12 Method", "Passed too many commands in setupHC12().");
                } else {
                    uah::prepareBuffer(
                        commandBuffer,
                        &commands[commandStartIndex],
                        (commandEndIndex - commandStartIndex),
                        SETUP_COMMAND_SIZE
                    );
                    xQueueSend(mSetupHC12CommandsQueue, commandBuffer, portMAX_DELAY);
                }
            }
        }
    }

    void HC12::firstChangeRFChannel(uint8_t channel) const {
        if (channel < DEFAULT_CHANNEL || channel > MAX_CHANNEL) {
            mpLogger->errorv("HC12 Method", "RF channel on HC12 module must be set between 1 - 127, but got:", channel);
            channel = DEFAULT_CHANNEL;
        }

        uint8_t commandBuffer[8];
        char messageBuffer[8];
        sprintf(messageBuffer, "HC+C%03u", channel);
        uah::prepareBuffer(commandBuffer, messageBuffer, SETUP_COMMAND_SIZE);
        setupHC12(commandBuffer);
    }

    void HC12::waitAndDisable() const {
        xSemaphoreTake(mSendingDataMutex, pdMS_TO_TICKS(POWER_MANAGEMENT_SEMAPHORE_TIMEOUT));
    }

    void HC12::onSleep(const bool turnOffRFModule) const {
        if (turnOffRFModule) {
            mpLogger->debug("HC12 onSleep", "HC12 sleep");
            setupHC12((uint8_t *) "HC+SLEEP");
        } else if (m_HC12_DATA.isPowerSavingEnabled) {
            mpLogger->debug("HC12 onSleep", "HC12 power saving");
            setupHC12((uint8_t *) "HC+FU2");
        } else {
            mpLogger->debug("HC12 onSleep", "HC12 none");
        }
    }

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

    // ========================== HC12 Main ===========================
    void HC12::hc12OutputDecider(const uint8_t *hc12Output, bool *isWaitingForSendConfirmation) const {
        if (*isWaitingForSendConfirmation) {
            *isWaitingForSendConfirmation = false;
            xTaskNotify(mTransmitTaskHandle, (uint32_t)*hc12Output, eSetValueWithOverwrite);
        } else if (uxSemaphoreGetCount(mSetupWorkingSemaphore) > 0) {
            xQueueSend(mSetupHC12ReceiveQueue, hc12Output, portMAX_DELAY);
        } else {
            mpCommunication->addByteToDecode(*hc12Output);
        }
    }

    void HC12::HC12MainTask(void *parameters) {
        auto &hc12 = *static_cast<HC12 *>(parameters);

        uint8_t status = DEFAULT_STATUS_NOTIF;
        bool isWaitingForSendConfirmation = false;

        hc12.createOnBootSetupTask();
        xSemaphoreTake(hc12.mSetupWorkingSemaphore, portMAX_DELAY);

        hc12.createSetupHC12Task();
        hc12.createTransmitTask();

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
                        hc12.hc12OutputDecider(&hc12Output, &isWaitingForSendConfirmation);
                    } else {
                        // delay for watchdog
                        vTaskDelay(pdMS_TO_TICKS(1));
                    }

                    // extra protection if somehow queue is not empty and task is suspended
                    if (uxQueueMessagesWaiting(hc12.mTransmitQueue) != 0 && hc12.mTransmitTaskHandle != nullptr) {
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
                HC12_MAIN_TASK_SIZE,
                this,
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

    // ========================== Send Task ===========================
    void HC12::transmitTask(void *parameters) {
        const auto &hc12 = *static_cast<HC12 *>(parameters);

        uint8_t transmitBuffer[PROTOCOL_SIZE];

        for (;;) {
            if (xQueueReceive(hc12.mTransmitQueue, transmitBuffer, pdMS_TO_TICKS(SUSPEND_TASK_TIME_SHORT)) == pdTRUE) {
                // if semaphore is signalizing that setup should be done first, wait until semaphore is back
                if (uxSemaphoreGetCount(hc12.mFirstSetupSemaphore) == 0) {
                    if (xSemaphoreTake(
                        hc12.mFirstSetupSemaphore,
                        pdMS_TO_TICKS(FIRST_SETUP_SEMAPHORE_TIMEOUT)
                    ) == pdFALSE) {
                        hc12.mpLogger->warning("HC12 FreeRTOS", "mFirstSetupSemaphore timeout");
                    }
                    xSemaphoreGive(hc12.mFirstSetupSemaphore);
                }

                xSemaphoreTake(hc12.mSendingDataMutex, portMAX_DELAY);

                // this delay is required for HC12 transmit/receive message properly
                vTaskDelay(pdMS_TO_TICKS(DELAY_BETWEEN_MESSAGES));

                uint32_t hc12Respond;
                // clearing old notification (if exist)
                xTaskNotifyWait(0, ULONG_MAX, &hc12Respond, 0);
                constexpr uint8_t notificationValue = WAITING_FOR_SEND_CONFIRMATION_NOTIF;
                xQueueSendToFront(hc12.mMainNotificationsQueue, &notificationValue, portMAX_DELAY);

                // transmitting data
                hc12.mpSerial->write(transmitBuffer, PROTOCOL_SIZE);

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
                TRANSMIT_TASK_SIZE,
                this,
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

    // ========================== Setup HC12 ==========================
    void HC12::setupHC12Task(void *parameters) {
        const auto &hc12 = *static_cast<HC12 *>(parameters);

        // prepare commandBuffer
        uint8_t commandBuffer[SETUP_COMMAND_SIZE] = {};

        for (;;) {
            if (xQueueReceive(hc12.mSetupHC12CommandsQueue, commandBuffer, portMAX_DELAY) == pdTRUE) {
                xSemaphoreTake(hc12.mSendingDataMutex, portMAX_DELAY);
                xSemaphoreGive(hc12.mSetupWorkingSemaphore);
                digitalWrite(hc12.m_HC12_DATA.setPin, LOW);
                vTaskDelay(pdMS_TO_TICKS(DELAY_AFTER_SET_PIN_LOW));


                hc12.mpLogger->verbosea("HC12 Setup", "Received command: ", commandBuffer, SETUP_COMMAND_SIZE);

                if (!(commandBuffer[0] == (uint8_t) 'H' && commandBuffer[1] == (uint8_t) 'C')) {
                    hc12.mpLogger->error("HC12 Setup", "Received array is not hc12 command.");
                } else {
                    const uint8_t lenOfCommand = uah::calcLenOfDataInArray(commandBuffer, SETUP_COMMAND_SIZE);
                    commandBuffer[0] = (uint8_t) 'A';
                    commandBuffer[1] = (uint8_t) 'T';

                    hc12.mpSerial->write(commandBuffer, lenOfCommand);

                    uint8_t hc12Response[SETUP_MAX_LEN_OF_RESPONSE];
                    uint8_t index = 0;
                    uah::clearBuffer(hc12Response, SETUP_MAX_LEN_OF_RESPONSE);

                    while (xQueueReceive(
                               hc12.mSetupHC12ReceiveQueue,
                               &hc12Response[index],
                               pdMS_TO_TICKS(RECEIVE_BYTE_TIMEOUT)
                           ) == pdTRUE) {
                        index++;
                    }

                    if (index == 0) {
                        hc12.mpLogger->error("HC12 Setup", "HC12 module is not responding.");
                    } else {
                        hc12.mpLogger->infoa("HC12 Setup", "HC12 response: ", hc12Response, SETUP_MAX_LEN_OF_RESPONSE);
                    }
                }

                digitalWrite(hc12.m_HC12_DATA.setPin, HIGH);
                vTaskDelay(pdMS_TO_TICKS(DELAY_AFTER_SET_PIN_HIGH));
                // clear random hc12 output after changing state of SET_PIN
                while (hc12.mpSerial->available() > 0) {
                    hc12.mpSerial->read();
                }
                xSemaphoreTake(hc12.mSetupWorkingSemaphore, 0);
                xSemaphoreGive(hc12.mSendingDataMutex);
            }
        }
    }

    void HC12::createSetupHC12Task() {
        if (mSetupHC12TaskHandle == nullptr) {
            xTaskCreate(
                setupHC12Task,
                "HC12 Setup Task",
                SETUP_HC12_TASK_SIZE,
                this,
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
    }

    std::array<char, 16> HC12::getHC12Response() const {
        constexpr uint8_t MAX_RESPONSE_WAIT_TIME = 100;

        mpSerial->flush();
        for (uint8_t i = 0; i < MAX_RESPONSE_WAIT_TIME; i++) {
            vTaskDelay(pdMS_TO_TICKS(1));
            if (mpSerial->available() > 0) break;
        }

        uint8_t index = 0;
        std::array<char, 16> receiveBuffer = {};
        while (mpSerial->available() > 0) {
            while (mpSerial->available() > 0) {
                receiveBuffer[index++] = static_cast<char>(mpSerial->read());
                if (index >= receiveBuffer.size() - 1) break;
            }
            if (index >= receiveBuffer.size() - 1) break;
            vTaskDelay(pdMS_TO_TICKS(5));
        }
        return receiveBuffer;
    }

    void HC12::onBootSetupTask(void *parameters) {
        constexpr std::string_view EXPECTED_RESPONSE_AT{"OK"};

        const auto &hc12 = *static_cast<HC12 *>(parameters);

        xSemaphoreTake(hc12.mSendingDataMutex, portMAX_DELAY);
        digitalWrite(hc12.m_HC12_DATA.setPin, LOW);
        vTaskDelay(pdMS_TO_TICKS(DELAY_AFTER_SET_PIN_LOW));

        // clear random hc12 output after powering on
        while (hc12.mpSerial->available() > 0) {
            hc12.mpSerial->read();
        }

        // check baud rates
        bool isHC12NotResponding = true;
        for (const auto baudRate: sm_BAUD_RATES) {
            hc12.mpSerial->updateBaudRate(baudRate);
            hc12.mpSerial->write("AT");

            std::array<char, 16> receiveBuffer = hc12.getHC12Response();

            if (strncmp(receiveBuffer.data(), EXPECTED_RESPONSE_AT.data(), EXPECTED_RESPONSE_AT.size()) == 0) {
                isHC12NotResponding = false;
                hc12.mpLogger->verbosev("HC12 onBootSetup", "Found HC12 baud rate: ", static_cast<int>(baudRate));

                vTaskDelay(DELAY_BETWEEN_MESSAGES);
                hc12.mpSerial->write("AT+DEFAULT");
                receiveBuffer = hc12.getHC12Response();
                hc12.mpLogger->debug("HC12 onBootSetup", receiveBuffer.data());

                const uint8_t channel = hc12.mpCommunication->getDefaultRfChannel();
                char sendBuffer[10] = {};
                sprintf(sendBuffer, "AT+C%03u", channel);
                vTaskDelay(DELAY_BETWEEN_MESSAGES);
                hc12.mpSerial->write(sendBuffer);
                receiveBuffer = hc12.getHC12Response();
                hc12.mpLogger->debug("HC12 onBootSetup", receiveBuffer.data());

                break;
            }
            vTaskDelay(DELAY_BETWEEN_MESSAGES);
        }
        hc12.mpSerial->updateBaudRate(sm_BAUD_RATES[ms_DEFAULT_BAUD_RATE_INDEX]);

        if (isHC12NotResponding) hc12.mpLogger->error("HC12 onBootSetup", "HC12 is not responding");

        digitalWrite(hc12.m_HC12_DATA.setPin, HIGH);
        vTaskDelay(pdMS_TO_TICKS(DELAY_AFTER_SET_PIN_HIGH));
        // clear random hc12 output after changing state of SET_PIN
        while (hc12.mpSerial->available() > 0) {
            hc12.mpSerial->read();
        }
        xSemaphoreGive(hc12.mSendingDataMutex);
        xSemaphoreGive(hc12.mSetupWorkingSemaphore);

        vTaskDelete(nullptr);
        for (;;) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    void HC12::createOnBootSetupTask() {
        xTaskCreate(
            onBootSetupTask,
            "HC12 On Boot Setup",
            ON_BOOT_SETUP_HC12_TASK_SIZE,
            this,
            MEDIUM_TASK_PRIORITY,
            nullptr
        );
    }

    HC12::HC12Data::HC12Data(const nl::json &data) : txPin(data[s_TX_PIN]),
                                                     rxPin(data[s_RX_PIN]),
                                                     setPin(data[s_SET_PIN]),
                                                     isPowerSavingEnabled(data[s_IS_POWER_SAVING_ENABLED]) {}
}
